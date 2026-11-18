#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <QRect>

namespace window_layout {

enum class Placement : std::uint8_t {
  LeftHalf,
  RightHalf,
  TopHalf,
  BottomHalf,
  CenterHalf,
  TopLeftQuarter,
  TopRightQuarter,
  BottomLeftQuarter,
  BottomRightQuarter,
  FirstThird,
  CenterThird,
  LastThird,
  FirstTwoThirds,
  LastTwoThirds,
  Maximize,
  AlmostMaximize,
  MaximizeHeight,
  MaximizeWidth,
  ReasonableSize,
  Center,
  MakeLarger,
  MakeSmaller,
  MoveLeft,
  MoveRight,
  MoveUp,
  MoveDown,
};

struct Options {
  /** Space applied around the screen edges and between tiles. */
  int gap = 0;
  /** Share of the work area used by `AlmostMaximize`, in both dimensions. */
  double almostMaximize = 0.95;
  /** Share of the work area used by `ReasonableSize`, in both dimensions. */
  double reasonableSize = 0.75;
};

/**
 * Target geometry for `window` inside `workArea`. Thirds are laid out vertically on portrait work areas.
 */
QRect place(Placement placement, const QRect &workArea, const QRect &window, const Options &options = {});

/**
 * Like `place`, but halves cycle through 1/2, 2/3 and 1/3 of the work area when used repeatedly: if
 * `window` already fills one of those sizes on that side, the next size is returned.
 */
QRect placeCycling(Placement placement, const QRect &workArea, const QRect &window,
                   const Options &options = {});

/**
 * Whether two window geometries match closely enough to be considered the same placement. Apps may
 * round requested sizes (terminals snap to their cell grid).
 */
bool approximatelyEqual(const QRect &a, const QRect &b);

/**
 * The screen the window belongs to: the one it overlaps the most, or the closest one when it
 * overlaps none.
 */
std::optional<std::size_t> screenIndexFor(const QRect &window, std::span<const QRect> screens);

/**
 * Map `window` from one work area to another, keeping its relative position and size.
 */
QRect moveBetween(const QRect &window, const QRect &from, const QRect &to);

} // namespace window_layout
