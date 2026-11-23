#include "internal/keyboard/key-grab.hpp"
#include <QCoreApplication>
#include <QtGlobal>

#ifdef Q_OS_MACOS
#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <optional>
#endif

#ifdef Q_OS_MACOS

struct KeyGrab::Private {
  static std::optional<Key> keyFor(CGKeyCode keycode) {
    switch (keycode) {
    case kVK_UpArrow:
      return Key::Up;
    case kVK_DownArrow:
      return Key::Down;
    case kVK_Escape:
      return Key::Escape;
    default:
      return std::nullopt;
    }
  }

  // Runs on the main thread, which owns the run loop the tap is added to.
  static CGEventRef callback(CGEventTapProxy, CGEventType type, CGEventRef event, void *userInfo) {
    auto *grab = static_cast<KeyGrab *>(userInfo);

    if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
      if (grab->m_tap) CGEventTapEnable(static_cast<CFMachPortRef>(grab->m_tap), true);
      return event;
    }

    const auto key =
        keyFor(static_cast<CGKeyCode>(CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode)));
    if (!key) return event;

    const bool repeat = CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat) != 0;
    if (type == kCGEventKeyDown && grab->m_handler && !(repeat && *key == Key::Escape)) {
      // Not from inside the callback: the handler may stop the grab, which releases this tap.
      QMetaObject::invokeMethod(
          QCoreApplication::instance(),
          [grab, key = *key]() {
            if (grab->m_handler) grab->m_handler(key);
          },
          Qt::QueuedConnection);
    }
    return nullptr;
  }
};

KeyGrab::~KeyGrab() { stop(); }

bool KeyGrab::start(Handler handler) {
  stop();

  const CGEventMask mask = CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp);
  CFMachPortRef tap = CGEventTapCreate(kCGHIDEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault, mask,
                                       &Private::callback, this);
  if (!tap) return false;

  CFRunLoopSourceRef source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap, 0);
  CFRunLoopAddSource(CFRunLoopGetMain(), source, kCFRunLoopCommonModes);
  CGEventTapEnable(tap, true);

  m_handler = std::move(handler);
  m_tap = tap;
  m_source = source;
  return true;
}

void KeyGrab::stop() {
  m_handler = nullptr;
  if (!m_tap) return;

  auto *tap = static_cast<CFMachPortRef>(m_tap);
  auto *source = static_cast<CFRunLoopSourceRef>(m_source);
  CGEventTapEnable(tap, false);
  CFRunLoopRemoveSource(CFRunLoopGetMain(), source, kCFRunLoopCommonModes);
  CFRelease(source);
  CFMachPortInvalidate(tap);
  CFRelease(tap);
  m_tap = nullptr;
  m_source = nullptr;
}

#else

KeyGrab::~KeyGrab() = default;

bool KeyGrab::start(Handler) { return false; }

void KeyGrab::stop() {}

#endif
