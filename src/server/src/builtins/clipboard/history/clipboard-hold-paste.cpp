#include "builtins/clipboard/history/clipboard-hold-paste.hpp"
#include <QCoreApplication>
#include <QDebug>
#include "builtins/clipboard/history/clipboard-history-view-host.hpp"
#include "internal/keyboard/modifier-state.hpp"
#include "navigation-controller.hpp"
#include "service-registry.hpp"
#include "services/global-shortcuts/global-shortcut-service.hpp"
#include "services/paste/paste-service.hpp"

ClipboardHoldPaste *ClipboardHoldPaste::instance() {
  static auto *const INSTANCE = new ClipboardHoldPaste(QCoreApplication::instance());
  return INSTANCE;
}

ClipboardHoldPaste::ClipboardHoldPaste(QObject *parent) : QObject(parent) {
  m_pollTimer.setInterval(POLL_INTERVAL);
  m_pollTimer.setTimerType(Qt::PreciseTimer);
  connect(&m_pollTimer, &QTimer::timeout, this, &ClipboardHoldPaste::poll);
}

bool ClipboardHoldPaste::shortcutPressed(const ApplicationContext &ctx, const EntrypointId &id) {
  if (m_state == State::Open && m_view) {
    m_view->holdSelectNext();
    return true;
  }

  finish();

  if (!ModifierState::isSupported() || !ModifierState::anyHeld()) return false;
  if (ctx.navigation->isWindowOpened() || !ctx.services->pasteService()->supportsPaste()) return false;

  m_ctx = &ctx;
  m_state = State::Opening;
  ctx.navigation->activateEntrypoint(
      id, {.props = {.source = LaunchSource::Shortcut}, .toggleIfAlreadyActive = false});

  if (m_state != State::Open) {
    qWarning() << "ClipboardHoldPaste: the history view did not open";
    finish();
  }
  return true;
}

void ClipboardHoldPaste::viewCreated(ClipboardHistoryViewHost *view) {
  if (m_state != State::Opening) return;
  m_view = view;
  m_state = State::Open;
  view->beginHoldMode();
  // Hotkeys bound to the held modifiers (window layouts on Hyper+arrows...) must not fire while picking,
  // and neither may the system's Control+arrow shortcuts.
  setShortcutsSuspended(true);
  m_keyGrab.start([this](KeyGrab::Key key) {
    if (!m_view) return;
    switch (key) {
    case KeyGrab::Key::Up:
      m_view->holdMove(-1);
      break;
    case KeyGrab::Key::Down:
      m_view->holdMove(1);
      break;
    case KeyGrab::Key::Escape:
      m_view->holdCancel();
      break;
    }
  });
  m_pollTimer.start();
}

void ClipboardHoldPaste::poll() {
  if (m_state != State::Open || !m_view || !m_view->inHoldMode() || !m_ctx->navigation->isWindowOpened()) {
    finish();
    return;
  }

  if (!ModifierState::anyHeld()) {
    const QPointer<ClipboardHistoryViewHost> view = m_view;
    finish();
    if (view) view->pasteHighlighted();
  }
}

void ClipboardHoldPaste::setShortcutsSuspended(bool suspended) {
  if (m_shortcutsSuspended == suspended) return;
  m_shortcutsSuspended = suspended;
  if (auto *shortcuts = ServiceRegistry::instance()->globalShortcuts()) shortcuts->setSuspended(suspended);
}

void ClipboardHoldPaste::finish() {
  m_pollTimer.stop();
  m_keyGrab.stop();
  setShortcutsSuspended(false);
  m_state = State::Idle;
  if (m_view && m_view->inHoldMode()) m_view->endHoldMode();
  m_view.clear();
  m_ctx = nullptr;
}
