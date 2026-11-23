#pragma once
#include <cstdint>

/**
 * Live state of the Command, Control, Option and Shift modifiers, for features that act when a hotkey's
 * modifiers are let go (hold to pick, release to confirm).
 *
 * Key remaps such as the Hyper key add modifiers to key events without the keys ever being physically
 * held, so the system does not see them: such remaps report their state here.
 */
namespace ModifierState {

/**
 * Whether the state can be observed on this platform.
 */
bool isSupported();

/**
 * Whether any modifier is held, physically or through a remap.
 */
bool anyHeld();

/**
 * The modifiers whose keys are physically down, as macOS CGEventFlags, read from the device-dependent bits
 * that only real modifier keys set. The device-independent bits (and the per key state the system derives
 * from them) also carry modifiers a remap added to events, and stay set after it lets go until the next key.
 * Always 0 on other platforms.
 */
std::uint64_t physicalFlags();

/**
 * Same as `physicalFlags()`, for the flags of a given event.
 */
std::uint64_t physicalFlags(std::uint64_t eventFlags);

/**
 * Report a remap holding synthesized modifiers. Thread safe.
 */
void setSynthesizedHeld(bool held);

} // namespace ModifierState
