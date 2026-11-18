#pragma once
#include <QCoreApplication>
#include "command/preference-schema.hpp"

struct WindowManagementPreferences {
  int windowGap = 0;
  bool cycleSizes = true;
  int almostMaximizeSize = 95;
  int reasonableSize = 75;
};

template <> struct PreferenceSchema<WindowManagementPreferences> {
  Q_DECLARE_TR_FUNCTIONS(WindowManagementPreferences)
public:
  PreferenceMeta windowGap{
      .title = tr("Window gap"),
      .description = tr("Space in pixels kept around windows arranged by layout commands"),
  };
  PreferenceMeta cycleSizes{
      .title = tr("Cycle sizes"),
      .description = tr("Repeating a half layout cycles the window through a half, two thirds and a third of "
                        "the screen"),
  };
  PreferenceMeta almostMaximizeSize{
      .title = tr("Almost Maximize size"),
      .description = tr("Percentage of the screen used by Almost Maximize"),
  };
  PreferenceMeta reasonableSize{
      .title = tr("Reasonable Size"),
      .description = tr("Percentage of the screen used by Reasonable Size"),
  };
};
