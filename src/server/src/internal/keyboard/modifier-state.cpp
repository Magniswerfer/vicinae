#include "internal/keyboard/modifier-state.hpp"
#include <atomic>
#include <QtGlobal>

#ifdef Q_OS_MACOS
#include <ApplicationServices/ApplicationServices.h>
#include <IOKit/hidsystem/IOLLEvent.h>
#include <array>
#include <utility>
#endif

namespace {
std::atomic<bool> synthesizedHeld{false};

#ifdef Q_OS_MACOS
constexpr auto DEVICE_MODIFIERS = std::to_array<std::pair<std::uint64_t, CGEventFlags>>({
    {NX_DEVICELCMDKEYMASK, kCGEventFlagMaskCommand},
    {NX_DEVICERCMDKEYMASK, kCGEventFlagMaskCommand},
    {NX_DEVICELCTLKEYMASK, kCGEventFlagMaskControl},
    {NX_DEVICERCTLKEYMASK, kCGEventFlagMaskControl},
    {NX_DEVICELALTKEYMASK, kCGEventFlagMaskAlternate},
    {NX_DEVICERALTKEYMASK, kCGEventFlagMaskAlternate},
    {NX_DEVICELSHIFTKEYMASK, kCGEventFlagMaskShift},
    {NX_DEVICERSHIFTKEYMASK, kCGEventFlagMaskShift},
});
#endif
} // namespace

namespace ModifierState {

bool isSupported() {
#ifdef Q_OS_MACOS
  return true;
#else
  return false;
#endif
}

bool anyHeld() { return synthesizedHeld.load() || physicalFlags() != 0; }

std::uint64_t physicalFlags() {
#ifdef Q_OS_MACOS
  return physicalFlags(CGEventSourceFlagsState(kCGEventSourceStateHIDSystemState));
#else
  return 0;
#endif
}

std::uint64_t physicalFlags(std::uint64_t eventFlags) {
  std::uint64_t flags = 0;
#ifdef Q_OS_MACOS
  for (const auto &[deviceMask, flag] : DEVICE_MODIFIERS) {
    if (eventFlags & deviceMask) flags |= flag;
  }
#else
  Q_UNUSED(eventFlags);
#endif
  return flags;
}

void setSynthesizedHeld(bool held) { synthesizedHeld.store(held); }

} // namespace ModifierState
