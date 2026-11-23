#pragma once
#include <cstdint>
#include <functional>

/**
 * Takes the arrow and Escape keys for ourselves, system wide, while active. They are caught before the
 * system's own shortcuts see them (Mission Control and Spaces use Control with the arrows), so they keep
 * working while modifiers that would otherwise trigger those are held.
 *
 * Only implemented on macOS, through an event tap: it needs the Accessibility permission.
 */
class KeyGrab {
public:
  enum class Key : std::uint8_t { Up, Down, Escape };
  using Handler = std::function<void(Key)>;

  KeyGrab() = default;
  ~KeyGrab();

  KeyGrab(const KeyGrab &) = delete;
  KeyGrab &operator=(const KeyGrab &) = delete;

  /**
   * The handler runs on the main thread. Returns false when the keys could not be grabbed.
   */
  bool start(Handler handler);
  void stop();
  bool isActive() const { return m_tap != nullptr; }

  struct Private;

private:
  Handler m_handler;
  void *m_tap = nullptr;    // CFMachPortRef
  void *m_source = nullptr; // CFRunLoopSourceRef
};
