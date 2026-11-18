#include <array>
#include <cstdlib>
#include <catch2/catch_test_macros.hpp>
#include <QRect>
#include "builtins/wm/window-layout.hpp"

using window_layout::Placement;

// NOLINTBEGIN(bugprone-throwing-static-initialization)
namespace {
const QRect WORK_AREA{0, 25, 1920, 1055};
const QRect WINDOW{300, 200, 800, 600};
} // namespace

TEST_CASE("halves split the work area without gaps") {
  REQUIRE(window_layout::place(Placement::LeftHalf, WORK_AREA, WINDOW) == QRect(0, 25, 960, 1055));
  REQUIRE(window_layout::place(Placement::RightHalf, WORK_AREA, WINDOW) == QRect(960, 25, 960, 1055));
  REQUIRE(window_layout::place(Placement::BottomHalf, WORK_AREA, WINDOW) == QRect(0, 553, 1920, 527));
}

TEST_CASE("gaps apply at screen edges and once between tiles") {
  const auto left = window_layout::place(Placement::LeftHalf, WORK_AREA, WINDOW, {.gap = 10});
  const auto right = window_layout::place(Placement::RightHalf, WORK_AREA, WINDOW, {.gap = 10});

  REQUIRE(left == QRect(10, 35, 945, 1035));
  REQUIRE(right.x() - (left.x() + left.width()) == 10);
  REQUIRE(right.x() + right.width() == 1910);
}

TEST_CASE("quarters cover the four corners") {
  REQUIRE(window_layout::place(Placement::TopLeftQuarter, WORK_AREA, WINDOW) == QRect(0, 25, 960, 528));
  REQUIRE(window_layout::place(Placement::BottomRightQuarter, WORK_AREA, WINDOW) ==
          QRect(960, 553, 960, 527));
}

TEST_CASE("thirds follow the work area orientation") {
  REQUIRE(window_layout::place(Placement::FirstThird, WORK_AREA, WINDOW) == QRect(0, 25, 640, 1055));
  REQUIRE(window_layout::place(Placement::LastTwoThirds, WORK_AREA, WINDOW) == QRect(640, 25, 1280, 1055));

  const QRect portrait{0, 0, 1080, 1920};
  REQUIRE(window_layout::place(Placement::CenterThird, portrait, WINDOW) == QRect(0, 640, 1080, 640));
}

TEST_CASE("maximize and center keep windows inside the work area") {
  REQUIRE(window_layout::place(Placement::Maximize, WORK_AREA, WINDOW) == WORK_AREA);
  REQUIRE(window_layout::place(Placement::Center, WORK_AREA, WINDOW) == QRect(560, 252, 800, 600));
  REQUIRE(window_layout::place(Placement::MaximizeHeight, WORK_AREA, WINDOW) == QRect(300, 25, 800, 1055));

  const QRect huge{-100, -100, 4000, 3000};
  REQUIRE(window_layout::place(Placement::Center, WORK_AREA, huge) == WORK_AREA);
}

TEST_CASE("moving keeps the size and snaps to the edge") {
  REQUIRE(window_layout::place(Placement::MoveRight, WORK_AREA, WINDOW) == QRect(1120, 200, 800, 600));
  REQUIRE(window_layout::place(Placement::MoveUp, WORK_AREA, WINDOW) == QRect(300, 25, 800, 600));
}

TEST_CASE("resizing grows around the center and never shrinks below the minimum") {
  const auto larger = window_layout::place(Placement::MakeLarger, WORK_AREA, WINDOW);
  REQUIRE(larger.width() == 800 + 192);
  REQUIRE(std::abs(larger.center().x() - WINDOW.center().x()) <= 1);

  const QRect small{100, 100, 250, 220};
  const auto smaller = window_layout::place(Placement::MakeSmaller, WORK_AREA, small);
  REQUIRE(smaller.width() == 200);
  REQUIRE(smaller.height() == 200);
}

TEST_CASE("the screen is picked by overlap, then by distance") {
  const auto screens = std::to_array<QRect>({{0, 0, 1920, 1080}, {1920, 0, 2560, 1440}});

  REQUIRE(window_layout::screenIndexFor({1800, 100, 800, 600}, screens) == 1);
  REQUIRE(window_layout::screenIndexFor({100, 100, 800, 600}, screens) == 0);
  REQUIRE(window_layout::screenIndexFor({5000, 100, 100, 100}, screens) == 1);
  REQUIRE_FALSE(window_layout::screenIndexFor(WINDOW, {}).has_value());
}

TEST_CASE("moving between displays keeps the relative layout") {
  const QRect from{0, 0, 1920, 1080};
  const QRect to{1920, 0, 2560, 1440};

  REQUIRE(window_layout::moveBetween({0, 0, 960, 1080}, from, to) == QRect(1920, 0, 1280, 1440));
}
TEST_CASE("repeating a half cycles through half, two thirds and a third") {
  const auto half = window_layout::placeCycling(Placement::LeftHalf, WORK_AREA, WINDOW);
  REQUIRE(half == QRect(0, 25, 960, 1055));

  const auto twoThirds = window_layout::placeCycling(Placement::LeftHalf, WORK_AREA, half);
  REQUIRE(twoThirds == QRect(0, 25, 1280, 1055));

  const auto third = window_layout::placeCycling(Placement::LeftHalf, WORK_AREA, twoThirds);
  REQUIRE(third == QRect(0, 25, 640, 1055));

  REQUIRE(window_layout::placeCycling(Placement::LeftHalf, WORK_AREA, third) == half);
}

TEST_CASE("cycling tolerates apps that round the requested size") {
  const QRect roundedHalf{0, 25, 952, 1050};
  REQUIRE(window_layout::placeCycling(Placement::LeftHalf, WORK_AREA, roundedHalf) ==
          QRect(0, 25, 1280, 1055));
}

TEST_CASE("right and bottom halves cycle from their own edge") {
  const auto half = window_layout::placeCycling(Placement::RightHalf, WORK_AREA, WINDOW);
  REQUIRE(window_layout::placeCycling(Placement::RightHalf, WORK_AREA, half) == QRect(640, 25, 1280, 1055));

  const auto bottom = window_layout::placeCycling(Placement::BottomHalf, WORK_AREA, WINDOW);
  REQUIRE(bottom == QRect(0, 553, 1920, 527));
}

TEST_CASE("placements without a cycle are unchanged") {
  REQUIRE(window_layout::placeCycling(Placement::Maximize, WORK_AREA, WINDOW) == WORK_AREA);
}
TEST_CASE("almost maximize and reasonable size use the configured share of the screen") {
  REQUIRE(window_layout::place(Placement::AlmostMaximize, WORK_AREA, WINDOW) == QRect(48, 51, 1824, 1002));
  REQUIRE(window_layout::place(Placement::ReasonableSize, WORK_AREA, WINDOW) == QRect(240, 157, 1440, 791));
  REQUIRE(window_layout::place(Placement::ReasonableSize, WORK_AREA, WINDOW, {.reasonableSize = 0.5}) ==
          QRect(480, 288, 960, 528));
}
// NOLINTEND(bugprone-throwing-static-initialization)
