#pragma once
#include <memory>
#include <glaze/core/meta.hpp>
#include <QCoreApplication>
#include <QPointer>
#include <QTimer>
#include "command/preference-schema.hpp"
#include "command/typed-command.hpp"
#include "services/builtin-icon/builtin-icon.hpp"
#include "services/hyper-key/macos-hyper-key.hpp"
#include "theme/colors.hpp"
#include "ui/image/url.hpp"

template <> struct glz::meta<MacosHyperKey::TapAction> {
  using enum MacosHyperKey::TapAction;
  static constexpr auto value = glz::enumerate("none", None, "escape", Escape);
};

struct HyperKeyPreferences {
  bool enabled = false;
  bool includeShift = true;
  MacosHyperKey::TapAction tapAction = MacosHyperKey::TapAction::None;
};

template <> struct PreferenceSchema<HyperKeyPreferences> {
  Q_DECLARE_TR_FUNCTIONS(HyperKeyPreferences)
public:
  PreferenceMeta enabled{
      .title = tr("Use Caps Lock as Hyper key"),
      .description = tr("Holding Caps Lock presses Command, Control, Option and Shift at once, so it can be "
                        "used in global shortcuts. Requires the Accessibility permission."),
  };
  PreferenceMeta includeShift{
      .title = tr("Include Shift"),
      .description = tr("Add Shift to the Hyper key. Turn off to use Command, Control and Option only."),
  };
  PreferenceMeta tapAction{
      .title = tr("Quick press"),
      .description = tr("What happens when Caps Lock is pressed and released without another key"),
      .options =
          [] {
            using Action = MacosHyperKey::TapAction;
            return std::vector<Preference::DropdownData::Option>{
                option(Action::None, tr("Nothing")),
                option(Action::Escape, tr("Escape")),
            };
          },
  };
};

class HyperKeyExtension : public TypedCommandRepository<HyperKeyPreferences> {
  Q_DECLARE_TR_FUNCTIONS(HyperKeyExtension)

public:
  HyperKeyExtension();
  ~HyperKeyExtension() override;

  QString id() const override { return "hyper-key"; }
  QString displayName() const override { return tr("Hyper Key"); }
  QString description() const override { return tr("Use Caps Lock as a Hyper key in global shortcuts"); }
  ImageURL iconUrl() const override {
    return ImageURL::builtin(BuiltinIcon::Keyboard).setBackgroundTint(SemanticColor::Purple);
  }

  void initialized(const HyperKeyPreferences &preferences) const override;
  void preferencesChanged(const HyperKeyPreferences &preferences) const override;

private:
  void reconcile() const;

  std::unique_ptr<MacosHyperKey> m_hyperKey;
  QPointer<QTimer> m_watchdog;
  mutable HyperKeyPreferences m_preferences;
  mutable bool m_prompted = false;
};
