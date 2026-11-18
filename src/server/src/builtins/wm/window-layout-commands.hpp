#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <QCoreApplication>
#include <QtGlobal>
#include "builtins/wm/window-layout.hpp"
#include "builtins/wm/wm-preferences.hpp"
#include "command/typed-command.hpp"
#include "services/builtin-icon/builtin-icon.hpp"
#include "theme/colors.hpp"
#include "ui/image/url.hpp"

struct ApplicationContext;

enum class WindowLayoutOperation : std::uint8_t { Place, NextDisplay, PreviousDisplay, Restore };

struct WindowLayoutSpec {
  const char *id;
  const char *name;
  BuiltinIcon icon;
  WindowLayoutOperation operation = WindowLayoutOperation::Place;
  window_layout::Placement placement = window_layout::Placement::Center;
};

namespace window_layout_detail {
using enum window_layout::Placement;
using Op = WindowLayoutOperation;

// Names are translated at runtime under the "WindowLayoutCommand" context.
constexpr auto SPECS = std::to_array<WindowLayoutSpec>({
    {"left-half", QT_TRANSLATE_NOOP("WindowLayoutCommand", "Left Half"), BuiltinIcon::AppWindowSidebarLeft,
     Op::Place, LeftHalf},
    {"right-half", QT_TRANSLATE_NOOP("WindowLayoutCommand", "Right Half"), BuiltinIcon::AppWindowSidebarRight,
     Op::Place, RightHalf},
    {"top-half", QT_TRANSLATE_NOOP("WindowLayoutCommand", "Top Half"), BuiltinIcon::ArrowUp, Op::Place,
     TopHalf},
    {"bottom-half", QT_TRANSLATE_NOOP("WindowLayoutCommand", "Bottom Half"), BuiltinIcon::ArrowDown,
     Op::Place, BottomHalf},
    {"center-half", QT_TRANSLATE_NOOP("WindowLayoutCommand", "Center Half"), BuiltinIcon::Center, Op::Place,
     CenterHalf},
    {"top-left-quarter", QT_TRANSLATE_NOOP("WindowLayoutCommand", "Top Left Quarter"),
     BuiltinIcon::AppWindowGrid2x2, Op::Place, TopLeftQuarter},
    {"top-right-quarter", QT_TRANSLATE_NOOP("WindowLayoutCommand", "Top Right Quarter"),
     BuiltinIcon::AppWindowGrid2x2, Op::Place, TopRightQuarter},
    {"bottom-left-quarter", QT_TRANSLATE_NOOP("WindowLayoutCommand", "Bottom Left Quarter"),
     BuiltinIcon::AppWindowGrid2x2, Op::Place, BottomLeftQuarter},
    {"bottom-right-quarter", QT_TRANSLATE_NOOP("WindowLayoutCommand", "Bottom Right Quarter"),
     BuiltinIcon::AppWindowGrid2x2, Op::Place, BottomRightQuarter},
    {"first-third", QT_TRANSLATE_NOOP("WindowLayoutCommand", "First Third"), BuiltinIcon::AppWindowGrid3x3,
     Op::Place, FirstThird},
    {"center-third", QT_TRANSLATE_NOOP("WindowLayoutCommand", "Center Third"), BuiltinIcon::AppWindowGrid3x3,
     Op::Place, CenterThird},
    {"last-third", QT_TRANSLATE_NOOP("WindowLayoutCommand", "Last Third"), BuiltinIcon::AppWindowGrid3x3,
     Op::Place, LastThird},
    {"first-two-thirds", QT_TRANSLATE_NOOP("WindowLayoutCommand", "First Two Thirds"),
     BuiltinIcon::AppWindowGrid3x3, Op::Place, FirstTwoThirds},
    {"last-two-thirds", QT_TRANSLATE_NOOP("WindowLayoutCommand", "Last Two Thirds"),
     BuiltinIcon::AppWindowGrid3x3, Op::Place, LastTwoThirds},
    {"maximize", QT_TRANSLATE_NOOP("WindowLayoutCommand", "Maximize"), BuiltinIcon::Maximize, Op::Place,
     Maximize},
    {"almost-maximize", QT_TRANSLATE_NOOP("WindowLayoutCommand", "Almost Maximize"),
     BuiltinIcon::ArrowsExpand, Op::Place, AlmostMaximize},
    {"maximize-height", QT_TRANSLATE_NOOP("WindowLayoutCommand", "Maximize Height"),
     BuiltinIcon::ArrowsExpand, Op::Place, MaximizeHeight},
    {"maximize-width", QT_TRANSLATE_NOOP("WindowLayoutCommand", "Maximize Width"), BuiltinIcon::ArrowsExpand,
     Op::Place, MaximizeWidth},
    {"reasonable-size", QT_TRANSLATE_NOOP("WindowLayoutCommand", "Reasonable Size"), BuiltinIcon::AppWindow,
     Op::Place, ReasonableSize},
    {"center", QT_TRANSLATE_NOOP("WindowLayoutCommand", "Center Window"), BuiltinIcon::Center, Op::Place,
     Center},
    {"make-larger", QT_TRANSLATE_NOOP("WindowLayoutCommand", "Make Larger"), BuiltinIcon::ArrowsExpand,
     Op::Place, MakeLarger},
    {"make-smaller", QT_TRANSLATE_NOOP("WindowLayoutCommand", "Make Smaller"), BuiltinIcon::ArrowsContract,
     Op::Place, MakeSmaller},
    {"move-left", QT_TRANSLATE_NOOP("WindowLayoutCommand", "Move Left"), BuiltinIcon::ArrowLeft, Op::Place,
     MoveLeft},
    {"move-right", QT_TRANSLATE_NOOP("WindowLayoutCommand", "Move Right"), BuiltinIcon::ArrowRight, Op::Place,
     MoveRight},
    {"move-up", QT_TRANSLATE_NOOP("WindowLayoutCommand", "Move Up"), BuiltinIcon::ArrowUp, Op::Place, MoveUp},
    {"move-down", QT_TRANSLATE_NOOP("WindowLayoutCommand", "Move Down"), BuiltinIcon::ArrowDown, Op::Place,
     MoveDown},
    {"next-display", QT_TRANSLATE_NOOP("WindowLayoutCommand", "Next Display"), BuiltinIcon::Monitor,
     Op::NextDisplay},
    {"previous-display", QT_TRANSLATE_NOOP("WindowLayoutCommand", "Previous Display"), BuiltinIcon::Monitor,
     Op::PreviousDisplay},
    {"restore", QT_TRANSLATE_NOOP("WindowLayoutCommand", "Restore Window"), BuiltinIcon::Undo, Op::Restore},
});
} // namespace window_layout_detail

void runWindowLayout(const ApplicationContext &ctx, const WindowLayoutSpec &spec,
                     const WindowManagementPreferences &preferences);

template <std::size_t Index>
class WindowLayoutCommand : public TypedCallbackCommand<NoPreferences, WindowManagementPreferences> {
  static constexpr const WindowLayoutSpec &SPEC = window_layout_detail::SPECS[Index];

public:
  QString id() const override { return SPEC.id; }
  QString name() const override { return QCoreApplication::translate("WindowLayoutCommand", SPEC.name); }
  std::vector<QString> keywords() const override { return {"window", "layout", "snap", "resize", "move"}; }
  ImageURL iconUrl() const override {
    return ImageURL::builtin(SPEC.icon).setBackgroundTint(SemanticColor::Blue);
  }

  void execute(const Controller &controller) const override {
    runWindowLayout(*controller.context(), SPEC, controller.repositoryPreferences());
  }
};
