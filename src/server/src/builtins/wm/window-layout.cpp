#include "builtins/wm/window-layout.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace window_layout {
namespace {

constexpr double RESIZE_STEP_RATIO = 0.1;
constexpr int MIN_WINDOW_SIZE = 200;
constexpr int SAME_PLACEMENT_TOLERANCE = 24;

struct Fractions {
  double x0;
  double x1;
  double y0;
  double y1;
};

int scaled(int extent, double ratio) { return static_cast<int>(std::lround(extent * ratio)); }

int edge(int origin, int extent, double fraction) { return origin + scaled(extent, fraction); }

// Outer edges get the full gap, inner edges half of it on each side, so adjacent tiles end up `gap`
// apart.
QRect tile(const QRect &area, Fractions f, int gap) {
  const int half = gap / 2;
  const int left = edge(area.x(), area.width(), f.x0) + (f.x0 <= 0.0 ? gap : gap - half);
  const int right = edge(area.x(), area.width(), f.x1) - (f.x1 >= 1.0 ? gap : half);
  const int top = edge(area.y(), area.height(), f.y0) + (f.y0 <= 0.0 ? gap : gap - half);
  const int bottom = edge(area.y(), area.height(), f.y1) - (f.y1 >= 1.0 ? gap : half);
  return {left, top, std::max(right - left, 1), std::max(bottom - top, 1)};
}

QRect inset(const QRect &area, int gap) { return tile(area, {.x0 = 0, .x1 = 1, .y0 = 0, .y1 = 1}, gap); }

QRect clampInto(const QRect &rect, const QRect &area) {
  const int width = std::min(rect.width(), area.width());
  const int height = std::min(rect.height(), area.height());
  const int x = std::clamp(rect.x(), area.x(), area.x() + area.width() - width);
  const int y = std::clamp(rect.y(), area.y(), area.y() + area.height() - height);
  return {x, y, width, height};
}

QRect centered(QSize size, const QRect &area) {
  const int width = std::min(size.width(), area.width());
  const int height = std::min(size.height(), area.height());
  return {area.x() + (area.width() - width) / 2, area.y() + (area.height() - height) / 2, width, height};
}

QRect resizedAroundCenter(const QRect &window, int width, int height) {
  const QPoint center = window.center();
  return {center.x() - width / 2, center.y() - height / 2, width, height};
}

Fractions thirds(const QRect &area, double from, double to) {
  if (area.height() > area.width()) return {.x0 = 0, .x1 = 1, .y0 = from, .y1 = to};
  return {.x0 = from, .x1 = to, .y0 = 0, .y1 = 1};
}

constexpr double HALF = 0.5;
constexpr double TWO_THIRDS = 2.0 / 3;
constexpr double ONE_THIRD = 1.0 / 3;

std::optional<std::array<Fractions, 3>> sizeCycle(Placement placement) {
  switch (placement) {
  case Placement::LeftHalf:
    return std::array<Fractions, 3>{{{.x0 = 0, .x1 = HALF, .y0 = 0, .y1 = 1},
                                     {.x0 = 0, .x1 = TWO_THIRDS, .y0 = 0, .y1 = 1},
                                     {.x0 = 0, .x1 = ONE_THIRD, .y0 = 0, .y1 = 1}}};
  case Placement::RightHalf:
    return std::array<Fractions, 3>{{{.x0 = HALF, .x1 = 1, .y0 = 0, .y1 = 1},
                                     {.x0 = ONE_THIRD, .x1 = 1, .y0 = 0, .y1 = 1},
                                     {.x0 = TWO_THIRDS, .x1 = 1, .y0 = 0, .y1 = 1}}};
  case Placement::TopHalf:
    return std::array<Fractions, 3>{{{.x0 = 0, .x1 = 1, .y0 = 0, .y1 = HALF},
                                     {.x0 = 0, .x1 = 1, .y0 = 0, .y1 = TWO_THIRDS},
                                     {.x0 = 0, .x1 = 1, .y0 = 0, .y1 = ONE_THIRD}}};
  case Placement::BottomHalf:
    return std::array<Fractions, 3>{{{.x0 = 0, .x1 = 1, .y0 = HALF, .y1 = 1},
                                     {.x0 = 0, .x1 = 1, .y0 = ONE_THIRD, .y1 = 1},
                                     {.x0 = 0, .x1 = 1, .y0 = TWO_THIRDS, .y1 = 1}}};
  default:
    return std::nullopt;
  }
}

std::int64_t distanceSquared(QPoint point, const QRect &rect) {
  const std::int64_t dx = std::max({rect.x() - point.x(), 0, point.x() - (rect.x() + rect.width())});
  const std::int64_t dy = std::max({rect.y() - point.y(), 0, point.y() - (rect.y() + rect.height())});
  return (dx * dx) + (dy * dy);
}

} // namespace

QRect place(Placement placement, const QRect &workArea, const QRect &window, const Options &options) {
  const int gap = options.gap;
  using enum Placement;
  const QRect inner = inset(workArea, gap);
  const QRect fitted = clampInto(window, inner);

  switch (placement) {
  case LeftHalf:
    return tile(workArea, {.x0 = 0, .x1 = 0.5, .y0 = 0, .y1 = 1}, gap);
  case RightHalf:
    return tile(workArea, {.x0 = 0.5, .x1 = 1, .y0 = 0, .y1 = 1}, gap);
  case TopHalf:
    return tile(workArea, {.x0 = 0, .x1 = 1, .y0 = 0, .y1 = 0.5}, gap);
  case BottomHalf:
    return tile(workArea, {.x0 = 0, .x1 = 1, .y0 = 0.5, .y1 = 1}, gap);
  case CenterHalf:
    return tile(workArea, {.x0 = 0.25, .x1 = 0.75, .y0 = 0, .y1 = 1}, gap);
  case TopLeftQuarter:
    return tile(workArea, {.x0 = 0, .x1 = 0.5, .y0 = 0, .y1 = 0.5}, gap);
  case TopRightQuarter:
    return tile(workArea, {.x0 = 0.5, .x1 = 1, .y0 = 0, .y1 = 0.5}, gap);
  case BottomLeftQuarter:
    return tile(workArea, {.x0 = 0, .x1 = 0.5, .y0 = 0.5, .y1 = 1}, gap);
  case BottomRightQuarter:
    return tile(workArea, {.x0 = 0.5, .x1 = 1, .y0 = 0.5, .y1 = 1}, gap);
  case FirstThird:
    return tile(workArea, thirds(workArea, 0, 1.0 / 3), gap);
  case CenterThird:
    return tile(workArea, thirds(workArea, 1.0 / 3, 2.0 / 3), gap);
  case LastThird:
    return tile(workArea, thirds(workArea, 2.0 / 3, 1), gap);
  case FirstTwoThirds:
    return tile(workArea, thirds(workArea, 0, 2.0 / 3), gap);
  case LastTwoThirds:
    return tile(workArea, thirds(workArea, 1.0 / 3, 1), gap);
  case Maximize:
    return inner;
  case AlmostMaximize:
    return centered(
        {scaled(workArea.width(), options.almostMaximize), scaled(workArea.height(), options.almostMaximize)},
        workArea);
  case MaximizeHeight:
    return {fitted.x(), inner.y(), fitted.width(), inner.height()};
  case MaximizeWidth:
    return {inner.x(), fitted.y(), inner.width(), fitted.height()};
  case ReasonableSize:
    return centered(
        {scaled(workArea.width(), options.reasonableSize), scaled(workArea.height(), options.reasonableSize)},
        workArea);
  case Center:
    return centered(fitted.size(), inner);
  case MakeLarger: {
    const int width = window.width() + scaled(workArea.width(), RESIZE_STEP_RATIO);
    const int height = window.height() + scaled(workArea.height(), RESIZE_STEP_RATIO);
    return clampInto(resizedAroundCenter(window, width, height), inner);
  }
  case MakeSmaller: {
    const int width = std::max(window.width() - scaled(workArea.width(), RESIZE_STEP_RATIO),
                               std::min(MIN_WINDOW_SIZE, window.width()));
    const int height = std::max(window.height() - scaled(workArea.height(), RESIZE_STEP_RATIO),
                                std::min(MIN_WINDOW_SIZE, window.height()));
    return clampInto(resizedAroundCenter(window, width, height), inner);
  }
  case MoveLeft:
    return {inner.x(), fitted.y(), fitted.width(), fitted.height()};
  case MoveRight:
    return {inner.x() + inner.width() - fitted.width(), fitted.y(), fitted.width(), fitted.height()};
  case MoveUp:
    return {fitted.x(), inner.y(), fitted.width(), fitted.height()};
  case MoveDown:
    return {fitted.x(), inner.y() + inner.height() - fitted.height(), fitted.width(), fitted.height()};
  }

  return window;
}

QRect placeCycling(Placement placement, const QRect &workArea, const QRect &window, const Options &options) {
  const auto cycle = sizeCycle(placement);
  if (!cycle) return place(placement, workArea, window, options);
  const int gap = options.gap;

  for (std::size_t index = 0; index < cycle->size(); ++index) {
    if (approximatelyEqual(window, tile(workArea, (*cycle)[index], gap))) {
      return tile(workArea, (*cycle)[(index + 1) % cycle->size()], gap);
    }
  }
  return tile(workArea, cycle->front(), gap);
}

bool approximatelyEqual(const QRect &a, const QRect &b) {
  return std::abs(a.x() - b.x()) <= SAME_PLACEMENT_TOLERANCE &&
         std::abs(a.y() - b.y()) <= SAME_PLACEMENT_TOLERANCE &&
         std::abs(a.width() - b.width()) <= SAME_PLACEMENT_TOLERANCE &&
         std::abs(a.height() - b.height()) <= SAME_PLACEMENT_TOLERANCE;
}

std::optional<std::size_t> screenIndexFor(const QRect &window, std::span<const QRect> screens) {
  std::optional<std::size_t> best;
  std::int64_t bestOverlap = 0;

  for (std::size_t index = 0; index < screens.size(); ++index) {
    const QRect overlap = window.intersected(screens[index]);
    const std::int64_t area = static_cast<std::int64_t>(overlap.width()) * overlap.height();
    if (area > bestOverlap) {
      bestOverlap = area;
      best = index;
    }
  }

  if (best) return best;

  std::int64_t bestDistance = std::numeric_limits<std::int64_t>::max();
  for (std::size_t index = 0; index < screens.size(); ++index) {
    const std::int64_t distance = distanceSquared(window.center(), screens[index]);
    if (distance < bestDistance) {
      bestDistance = distance;
      best = index;
    }
  }

  return best;
}

QRect moveBetween(const QRect &window, const QRect &from, const QRect &to) {
  if (from.width() <= 0 || from.height() <= 0) return clampInto(window, to);

  const double sx = static_cast<double>(to.width()) / from.width();
  const double sy = static_cast<double>(to.height()) / from.height();

  const QRect mapped(to.x() + scaled(window.x() - from.x(), sx), to.y() + scaled(window.y() - from.y(), sy),
                     scaled(window.width(), sx), scaled(window.height(), sy));
  return clampInto(mapped, to);
}

} // namespace window_layout
