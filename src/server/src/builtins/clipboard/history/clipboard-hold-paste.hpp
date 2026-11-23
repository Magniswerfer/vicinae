#pragma once
#include <chrono>
#include <cstdint>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include "common/entrypoint.hpp"
#include "internal/keyboard/key-grab.hpp"

struct ApplicationContext;
class ClipboardHistoryViewHost;

/**
 * Cmd+Tab style quick paste for the clipboard history hotkey. The hotkey opens the history while its
 * modifiers stay held, pressing its key again or the arrows moves the highlight, and letting go of the
 * modifiers pastes the highlighted item. A quick tap pastes the latest item.
 */
class ClipboardHoldPaste : public QObject {
  Q_OBJECT

public:
  static ClipboardHoldPaste *instance();

  /**
   * Returns false when the press should launch the history as usual.
   */
  bool shortcutPressed(const ApplicationContext &ctx, const EntrypointId &id);

  /**
   * Called by every new history view, so the one opened by the hotkey can take part.
   */
  void viewCreated(ClipboardHistoryViewHost *view);

private:
  enum class State : std::uint8_t { Idle, Opening, Open };

  static constexpr std::chrono::milliseconds POLL_INTERVAL{10};

  explicit ClipboardHoldPaste(QObject *parent);

  void poll();
  void finish();
  void setShortcutsSuspended(bool suspended);

  State m_state = State::Idle;
  const ApplicationContext *m_ctx = nullptr;
  bool m_shortcutsSuspended = false;
  KeyGrab m_keyGrab;
  QTimer m_pollTimer;
  QPointer<ClipboardHistoryViewHost> m_view;
};
