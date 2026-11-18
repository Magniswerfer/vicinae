#pragma once
#include "builtins/wm/wm-preferences.hpp"
#include "command/command-database.hpp"
#include "command/typed-command.hpp"
#include "service-registry.hpp"
#include "ui/image/url.hpp"
#include <qcontainerfwd.h>
#include <QCoreApplication>

class WindowManagementExtension : public TypedCommandRepository<WindowManagementPreferences> {
  QString id() const override;
  QString displayName() const override;
  ImageURL iconUrl() const override;

public:
  WindowManagementExtension(const ServiceRegistry &services);
};
