#pragma once
#include <cstdint>
#include <expected>
#include <memory>
#include <thread>
#include <QString>

/**
 * Turns Caps Lock into a Hyper key (Cmd+Ctrl+Opt+Shift) on macOS.
 *
 * Caps Lock cannot be observed reliably (it toggles a lock state, has no key up and has a hardware
 * activation delay), so it is first remapped to F18 through the HID event system `UserKeyMapping`
 * property, the same mechanism `hidutil` uses. An active event tap then swallows F18 and adds the Hyper
 * modifiers to every key event while it is held. Because the modifiers are added before the window
 * server matches hot keys, Hyper combinations work for Carbon global shortcuts like any other modifier.
 * Shift+Caps Lock toggles the real Caps Lock state.
 *
 * The tap runs on its own high priority thread so key delivery never waits on the main thread. It needs
 * the Accessibility permission.
 */
class MacosHyperKey {
public:
  enum class TapAction : std::uint8_t { None, Escape };

  struct Options {
    bool includeShift = true;
    TapAction tapAction = TapAction::None;
  };

  struct TapState;

  MacosHyperKey();
  ~MacosHyperKey();

  MacosHyperKey(const MacosHyperKey &) = delete;
  MacosHyperKey &operator=(const MacosHyperKey &) = delete;

  /**
   * Start, or update the options of the running Hyper key. Fails when the event tap cannot be created,
   * usually because the Accessibility permission is missing, in which case Caps Lock is left untouched.
   */
  std::expected<void, QString> start(const Options &options);
  void stop();
  bool isRunning() const { return m_thread.joinable(); }

  /**
   * Whether the event tap is still receiving events. macOS disables it when the Accessibility permission
   * is revoked, and it does not come back on its own when the permission is granted again.
   */
  bool isHealthy() const;

  /**
   * Remove the Caps Lock remap left behind by a previous run that did not exit cleanly.
   */
  static void clearStaleRemap();

private:
  void applyOptions(const Options &options);
  void installRemap();
  void removeRemap();

  std::unique_ptr<TapState> m_state;
  std::thread m_thread;
  void *m_activity = nullptr;     // id<NSObject>, retained
  void *m_previousCaps = nullptr; // NSDictionary *, retained: user mapping we replaced
  bool m_remapped = false;
};
