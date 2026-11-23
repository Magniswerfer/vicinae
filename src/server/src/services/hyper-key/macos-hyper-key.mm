#include "services/hyper-key/macos-hyper-key.hpp"

#import <Foundation/Foundation.h>
#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <IOKit/hid/IOHIDProperties.h>
#include <IOKit/hidsystem/IOHIDEventSystemClient.h>
#include <IOKit/hidsystem/IOHIDLib.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#include <pthread.h>

#include <atomic>
#include <chrono>
#include <future>
#include <QCoreApplication>
#include <QDebug>

namespace {

constexpr std::uint64_t CAPS_LOCK_USAGE = 0x700000039;
constexpr std::uint64_t F18_USAGE = 0x70000006D;
constexpr CGKeyCode F18_KEYCODE = kVK_F18;
constexpr auto TAP_TIMEOUT = std::chrono::milliseconds(500);
constexpr CGEventFlags HYPER_FLAGS_NO_SHIFT =
    kCGEventFlagMaskCommand | kCGEventFlagMaskControl | kCGEventFlagMaskAlternate;

} // namespace

struct MacosHyperKey::TapState {
  std::atomic<CGEventFlags> flags{HYPER_FLAGS_NO_SHIFT | kCGEventFlagMaskShift};
  std::atomic<TapAction> tapAction{TapAction::None};
  std::atomic<bool> stopping{false};
  std::atomic<CFRunLoopRef> runLoop{nullptr};
  std::atomic<CFMachPortRef> tap{nullptr};

  // Only touched on the tap thread.
  bool hyperDown = false;
  bool used = false;
  bool capsToggleDown = false;
  std::chrono::steady_clock::time_point pressedAt;
  io_connect_t hidSystem = IO_OBJECT_NULL;
};

namespace {

void toggleCapsLock(MacosHyperKey::TapState &state) {
  if (state.hidSystem == IO_OBJECT_NULL) {
    io_service_t service =
        IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching(kIOHIDSystemClass));
    if (!service) return;
    IOServiceOpen(service, mach_task_self(), kIOHIDParamConnectType, &state.hidSystem);
    IOObjectRelease(service);
    if (state.hidSystem == IO_OBJECT_NULL) return;
  }

  bool locked = false;
  if (IOHIDGetModifierLockState(state.hidSystem, kIOHIDCapsLockState, &locked) == KERN_SUCCESS) {
    IOHIDSetModifierLockState(state.hidSystem, kIOHIDCapsLockState, !locked);
  }
}

void postKeyPress(CGEventTapProxy proxy, CGKeyCode key) {
  for (const bool down : {true, false}) {
    CGEventRef event = CGEventCreateKeyboardEvent(nullptr, key, down);
    if (!event) return;
    CGEventTapPostEvent(proxy, event);
    CFRelease(event);
  }
}

CGEventRef tapCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *userInfo) {
  auto &state = *static_cast<MacosHyperKey::TapState *>(userInfo);

  if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
    state.hyperDown = false;
    state.capsToggleDown = false;
    if (CFMachPortRef tap = state.tap.load()) CGEventTapEnable(tap, true);
    return event;
  }

  if (type == kCGEventFlagsChanged) {
    if (state.hyperDown) CGEventSetFlags(event, CGEventGetFlags(event) | state.flags.load());
    return event;
  }

  const auto keycode = static_cast<CGKeyCode>(CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));

  if (keycode == F18_KEYCODE) {
    const auto now = std::chrono::steady_clock::now();
    if (type == kCGEventKeyDown) {
      if (state.capsToggleDown) return nullptr;
      if (!state.hyperDown && (CGEventGetFlags(event) & kCGEventFlagMaskShift)) {
        state.capsToggleDown = true;
        toggleCapsLock(state);
        return nullptr;
      }
      if (!state.hyperDown) {
        state.hyperDown = true;
        state.used = false;
        state.pressedAt = now;
      }
      return nullptr;
    }

    if (state.capsToggleDown) {
      state.capsToggleDown = false;
      return nullptr;
    }

    const bool tapped = state.hyperDown && !state.used && now - state.pressedAt < TAP_TIMEOUT;
    state.hyperDown = false;
    if (tapped && state.tapAction.load() == MacosHyperKey::TapAction::Escape) {
      postKeyPress(proxy, kVK_Escape);
    }
    return nullptr;
  }

  if (state.hyperDown) {
    if (type == kCGEventKeyDown) state.used = true;
    CGEventSetFlags(event, CGEventGetFlags(event) | state.flags.load());
  }

  return event;
}

CFMachPortRef createTap(MacosHyperKey::TapState &state) {
  const CGEventMask mask =
      CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp) | CGEventMaskBit(kCGEventFlagsChanged);

  for (const CGEventTapLocation location : {kCGHIDEventTap, kCGSessionEventTap}) {
    if (CFMachPortRef tap = CGEventTapCreate(location, kCGHeadInsertEventTap, kCGEventTapOptionDefault, mask,
                                             &tapCallback, &state)) {
      return tap;
    }
  }
  return nullptr;
}

void runTap(MacosHyperKey::TapState &state, std::promise<bool> &ready) {
  pthread_setname_np("vicinae-hyper-key");
  pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);

  CFMachPortRef tap = createTap(state);
  if (!tap) {
    ready.set_value(false);
    return;
  }

  CFRunLoopRef loop = CFRunLoopGetCurrent();
  CFRunLoopSourceRef source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap, 0);
  CFRunLoopAddSource(loop, source, kCFRunLoopCommonModes);
  state.tap = tap;
  state.hyperDown = false;
  state.capsToggleDown = false;
  state.runLoop = static_cast<CFRunLoopRef>(const_cast<void *>(CFRetain(loop)));
  CGEventTapEnable(tap, true);
  ready.set_value(true);

  // Polled so a stop requested before the loop starts running is never missed.
  while (!state.stopping) {
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
  }

  CGEventTapEnable(tap, false);
  CFRunLoopRemoveSource(loop, source, kCFRunLoopCommonModes);
  CFRelease(source);
  state.tap = nullptr;
  CFMachPortInvalidate(tap);
  CFRelease(tap);
  CFRelease(state.runLoop.exchange(nullptr));
  if (state.hidSystem != IO_OBJECT_NULL) {
    IOServiceClose(state.hidSystem);
    state.hidSystem = IO_OBJECT_NULL;
  }
}

NSString *const SRC_KEY = @kIOHIDKeyboardModifierMappingSrcKey;
NSString *const DST_KEY = @kIOHIDKeyboardModifierMappingDstKey;

bool isCapsLockMapping(id entry) {
  return [entry isKindOfClass:NSDictionary.class] &&
         [((NSDictionary *)entry)[SRC_KEY] unsignedLongLongValue] == CAPS_LOCK_USAGE;
}

bool isHyperMapping(id entry) {
  return isCapsLockMapping(entry) && [((NSDictionary *)entry)[DST_KEY] unsignedLongLongValue] == F18_USAGE;
}

NSArray *copyUserKeyMappings(IOHIDEventSystemClientRef client) {
  id value = CFBridgingRelease(IOHIDEventSystemClientCopyProperty(client, CFSTR(kIOHIDUserKeyUsageMapKey)));
  return [value isKindOfClass:NSArray.class] ? value : @[];
}

bool setUserKeyMappings(IOHIDEventSystemClientRef client, NSArray *mappings) {
  return IOHIDEventSystemClientSetProperty(client, CFSTR(kIOHIDUserKeyUsageMapKey),
                                           (__bridge CFArrayRef)mappings);
}

template <typename Fn> void updateUserKeyMappings(Fn &&transform) {
  IOHIDEventSystemClientRef client = IOHIDEventSystemClientCreateSimpleClient(kCFAllocatorDefault);
  if (!client) {
    qWarning() << "MacosHyperKey: could not create HID event system client";
    return;
  }

  NSArray *current = copyUserKeyMappings(client);
  NSArray *next = transform(current);
  if (![next isEqualToArray:current] && !setUserKeyMappings(client, next)) {
    qWarning() << "MacosHyperKey: failed to update UserKeyMapping";
  }

  CFRelease(client);
}

} // namespace

MacosHyperKey::MacosHyperKey() : m_state(std::make_unique<TapState>()) {}

MacosHyperKey::~MacosHyperKey() { stop(); }

void MacosHyperKey::applyOptions(const Options &options) {
  m_state->flags =
      options.includeShift ? (HYPER_FLAGS_NO_SHIFT | kCGEventFlagMaskShift) : HYPER_FLAGS_NO_SHIFT;
  m_state->tapAction = options.tapAction;
}

std::expected<void, QString> MacosHyperKey::start(const Options &options) {
  applyOptions(options);
  if (isRunning()) return {};

  m_state->stopping = false;
  std::promise<bool> ready;
  auto created = ready.get_future();
  m_thread =
      std::thread([state = m_state.get(), ready = std::move(ready)]() mutable { runTap(*state, ready); });

  if (!created.get()) {
    m_thread.join();
    return std::unexpected(QCoreApplication::translate(
        "MacosHyperKey", "Vicinae needs the Accessibility permission to use Caps Lock as a Hyper key"));
  }

  installRemap();
  m_activity = (void *)CFBridgingRetain([[NSProcessInfo processInfo]
      beginActivityWithOptions:NSActivityLatencyCritical | NSActivityUserInitiatedAllowingIdleSystemSleep
                        reason:@"Hyper key event tap"]);
  return {};
}

bool MacosHyperKey::isHealthy() const {
  CFMachPortRef tap = m_state->tap.load();
  return isRunning() && tap && CGEventTapIsEnabled(tap);
}

void MacosHyperKey::stop() {
  if (!isRunning()) return;

  removeRemap();

  m_state->stopping = true;
  if (CFRunLoopRef loop = m_state->runLoop.load()) {
    CFRunLoopStop(loop);
    CFRunLoopWakeUp(loop);
  }
  m_thread.join();

  if (m_activity) {
    [[NSProcessInfo processInfo] endActivity:(id)CFBridgingRelease(m_activity)];
    m_activity = nullptr;
  }
}

void MacosHyperKey::installRemap() {
  updateUserKeyMappings([this](NSArray *current) {
    NSMutableArray *next = [NSMutableArray arrayWithCapacity:current.count + 1];
    for (id entry in current) {
      if (isCapsLockMapping(entry)) {
        if (!isHyperMapping(entry) && !m_previousCaps) m_previousCaps = (void *)CFBridgingRetain(entry);
        continue;
      }
      [next addObject:entry];
    }
    [next addObject:@{SRC_KEY : @(CAPS_LOCK_USAGE), DST_KEY : @(F18_USAGE)}];
    return next;
  });
  // Also true when the mapping was already there (left behind by an unclean exit): still ours to undo.
  m_remapped = true;
}

void MacosHyperKey::removeRemap() {
  if (!m_remapped) return;

  NSDictionary *previous = m_previousCaps ? (NSDictionary *)CFBridgingRelease(m_previousCaps) : nil;
  m_previousCaps = nullptr;

  updateUserKeyMappings([previous](NSArray *current) {
    NSMutableArray *next = [NSMutableArray arrayWithCapacity:current.count + 1];
    for (id entry in current) {
      if (!isHyperMapping(entry)) [next addObject:entry];
    }
    if (previous) [next addObject:previous];
    return next;
  });
  m_remapped = false;
}

void MacosHyperKey::clearStaleRemap() {
  updateUserKeyMappings([](NSArray *current) {
    NSMutableArray *next = [NSMutableArray arrayWithCapacity:current.count];
    for (id entry in current) {
      if (!isHyperMapping(entry)) [next addObject:entry];
    }
    return next;
  });
}
