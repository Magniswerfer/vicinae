#include "builtins/hyper-key/hyper-key-extension.hpp"
#include <chrono>
#include <QDebug>
#include <QTimer>
#include "service-registry.hpp"
#include "services/permissions/macos-permission-service.hpp"
#include "services/toast/toast-service.hpp"

namespace {
constexpr auto WATCHDOG_INTERVAL = std::chrono::seconds(2);
}

HyperKeyExtension::HyperKeyExtension()
    : m_hyperKey(std::make_unique<MacosHyperKey>()), m_watchdog(new QTimer(QCoreApplication::instance())) {
  m_watchdog->setInterval(WATCHDOG_INTERVAL);
  QObject::connect(m_watchdog.get(), &QTimer::timeout, m_watchdog.get(), [this]() { reconcile(); });

  // Caps Lock must never stay remapped once we stop handling it.
  QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, m_watchdog.get(),
                   [hyperKey = m_hyperKey.get()]() { hyperKey->stop(); });
}

HyperKeyExtension::~HyperKeyExtension() {
  m_hyperKey->stop();
  if (m_watchdog) m_watchdog->deleteLater();
}

void HyperKeyExtension::initialized(const HyperKeyPreferences &preferences) const {
  if (!preferences.enabled) MacosHyperKey::clearStaleRemap();
}

void HyperKeyExtension::preferencesChanged(const HyperKeyPreferences &preferences) const {
  m_preferences = preferences;
  m_prompted = false;
  reconcile();
}

// Runs on every preference change and periodically while enabled, so the Hyper key follows the
// Accessibility permission being granted, revoked or re-granted.
void HyperKeyExtension::reconcile() const {
  if (!m_preferences.enabled) {
    m_watchdog->stop();
    m_hyperKey->stop();
    return;
  }

  if (!m_watchdog->isActive()) m_watchdog->start();

  const bool accessibility = vicinae::permissions::accessibilityGranted();
  const bool inputMonitoring = vicinae::permissions::inputMonitoringGranted();

  if (!accessibility || !inputMonitoring) {
    // Without a working tap Caps Lock would be a dead F18 key: give it back until the permissions return.
    m_hyperKey->stop();
    if (!m_prompted) {
      m_prompted = true;
      qWarning() << "HyperKeyExtension: missing permissions, accessibility:" << accessibility
                 << "input monitoring:" << inputMonitoring;
      ServiceRegistry::instance()->toastService()->failure(
          tr("Hyper key unavailable"),
          tr("Allow Vicinae in Accessibility and Input Monitoring settings to use the Hyper key"));
      if (!accessibility) vicinae::permissions::requestAccessibility();
      if (!inputMonitoring) vicinae::permissions::requestInputMonitoring();
    }
    return;
  }

  if (m_hyperKey->isRunning() && !m_hyperKey->isHealthy()) {
    qInfo() << "HyperKeyExtension: event tap was disabled, restarting it";
    m_hyperKey->stop();
  }

  if (const auto started = m_hyperKey->start(
          {.includeShift = m_preferences.includeShift, .tapAction = m_preferences.tapAction});
      !started) {
    qWarning() << "HyperKeyExtension:" << started.error();
  }
}
