#include "builtins/wm/window-layout-commands.hpp"
#include <algorithm>
#include <numeric>
#include <optional>
#include <ranges>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>
#include "common/context.hpp"
#include "navigation-controller.hpp"
#include "service-registry.hpp"
#include "services/permissions/macos-permission-service.hpp"
#include "services/window-manager/abstract-window-manager.hpp"
#include "services/window-manager/window-manager.hpp"

namespace {

using WindowBounds = AbstractWindowManager::WindowBounds;

struct RestoreEntry {
  QRect original;
  QRect applied;
};

std::unordered_map<QString, RestoreEntry> &restoreHistory() {
  static std::unordered_map<QString, RestoreEntry> history;
  return history;
}

QRect toRect(const WindowBounds &bounds) { return {bounds.x, bounds.y, bounds.width, bounds.height}; }

WindowBounds toBounds(const QRect &rect) {
  return {.x = rect.x(), .y = rect.y(), .width = rect.width(), .height = rect.height()};
}

std::optional<QRect> targetFor(const WindowLayoutSpec &spec, const QString &windowId, const QRect &current,
                               std::span<const QRect> workAreas, const window_layout::Options &options,
                               bool cycleSizes) {
  const auto screenIndex = window_layout::screenIndexFor(current, workAreas);
  if (!screenIndex) return std::nullopt;
  const QRect &workArea = workAreas[*screenIndex];

  switch (spec.operation) {
  case WindowLayoutOperation::Place:
    return cycleSizes ? window_layout::placeCycling(spec.placement, workArea, current, options)
                      : window_layout::place(spec.placement, workArea, current, options);
  case WindowLayoutOperation::NextDisplay:
  case WindowLayoutOperation::PreviousDisplay: {
    if (workAreas.size() < 2) return std::nullopt;
    // Order displays left to right, then top to bottom, so cycling follows the physical arrangement.
    std::vector<std::size_t> order(workAreas.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::ranges::sort(order, [&](std::size_t a, std::size_t b) {
      const QRect &ra = workAreas[a];
      const QRect &rb = workAreas[b];
      return std::pair(ra.x(), ra.y()) < std::pair(rb.x(), rb.y());
    });
    const auto position = static_cast<std::size_t>(std::ranges::find(order, *screenIndex) - order.begin());
    const std::size_t step = spec.operation == WindowLayoutOperation::NextDisplay ? 1 : order.size() - 1;
    const std::size_t target = order[(position + step) % order.size()];
    return window_layout::moveBetween(current, workArea, workAreas[target]);
  }
  case WindowLayoutOperation::Restore: {
    auto &history = restoreHistory();
    const auto it = history.find(windowId);
    if (it == history.end()) return std::nullopt;
    return it->second.original;
  }
  }

  return std::nullopt;
}

} // namespace

void runWindowLayout(const ApplicationContext &ctx, const WindowLayoutSpec &spec,
                     const WindowManagementPreferences &preferences) {
  auto *wm = ctx.services->windowManager()->provider();
  auto &navigation = *ctx.navigation;

  navigation.closeWindow();

  if (!vicinae::permissions::requestAccessibility()) {
    navigation.showHud(QCoreApplication::translate(
        "WindowLayoutCommand", "Allow Vicinae in Accessibility settings to move windows"));
    return;
  }

  const auto window = wm->getFocusedWindowSync();
  if (!window) {
    navigation.showHud(QCoreApplication::translate("WindowLayoutCommand", "No focused window"));
    return;
  }
  if (window->fullScreen()) {
    navigation.showHud(QCoreApplication::translate("WindowLayoutCommand", "Window is in full screen"));
    return;
  }
  const auto bounds = window->bounds();
  if (!bounds) {
    navigation.showHud(QCoreApplication::translate("WindowLayoutCommand", "Could not read window position"));
    return;
  }

  const auto workAreas = wm->listScreensSync() |
                         std::views::transform([](const auto &screen) { return screen.workArea; }) |
                         std::ranges::to<std::vector>();

  const QRect current = toRect(*bounds);
  const auto percent = [](int value) { return std::clamp(value, 10, 100) / 100.0; };
  const window_layout::Options options{.gap = std::max(preferences.windowGap, 0),
                                       .almostMaximize = percent(preferences.almostMaximizeSize),
                                       .reasonableSize = percent(preferences.reasonableSize)};
  const auto target = targetFor(spec, window->id(), current, workAreas, options, preferences.cycleSizes);

  if (!target) {
    if (spec.operation == WindowLayoutOperation::Restore)
      navigation.showHud(QCoreApplication::translate("WindowLayoutCommand", "Nothing to restore"));
    return;
  }
  if (*target == current) return;

  auto &history = restoreHistory();
  if (spec.operation == WindowLayoutOperation::Restore) {
    history.erase(window->id());
  } else {
    auto [it, inserted] = history.try_emplace(window->id(), RestoreEntry{.original = current});
    // The window was moved by hand since we last placed it: that is the new state to restore to.
    if (!inserted && !window_layout::approximatelyEqual(current, it->second.applied))
      it->second.original = current;
    it->second.applied = *target;
  }

  if (!wm->setWindowBounds(*window, toBounds(*target))) {
    navigation.showHud(
        QCoreApplication::translate("WindowLayoutCommand", "This window cannot be moved or resized"));
  }
}
