#pragma once

/**
 * @file CircleScene.h
 * @brief 화면 상태(클릭 지점 최대 3개)와 그리기 규칙. UI(MFC)와 분리되어 단위 테스트가 가능하다.
 */

#include "Geometry.h"
#include "GrayCanvas.h"

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

namespace circle {

/// 사용자 입력으로 정해지는 그리기 설정.
struct DrawSettings {
    static constexpr int MIN_POINT_RADIUS = 1;
    static constexpr int MAX_POINT_RADIUS = 100;
    static constexpr int MIN_THICKNESS    = 1;
    static constexpr int MAX_THICKNESS    = 50;

    int pointRadius     = 10;  ///< 클릭 지점 원의 반지름 (px)
    int circleThickness = 3;   ///< 정원 가장자리 두께 (px)
};

class CircleScene {
public:
    static constexpr std::size_t MAX_POINTS = 3;

    void reset() noexcept { m_points.clear(); }

    /// 점을 추가한다. 이미 3개면 추가하지 않고 false (4번째 클릭부터는 클릭 지점 원을 그리지 않음).
    bool addPoint(const Point2D& point);

    /// point 에서 grabRadius 이내인 점 중 가장 가까운 점의 인덱스.
    std::optional<std::size_t> hitTest(const Point2D& point, double grabRadius) const noexcept;

    /// @throws std::out_of_range index 가 범위 밖인 경우.
    void movePoint(std::size_t index, const Point2D& point);

    void setPoints(const std::array<Point2D, MAX_POINTS>& points);

    const std::vector<Point2D>& points() const noexcept { return m_points; }
    bool isComplete() const noexcept { return m_points.size() == MAX_POINTS; }

    /// 점이 3개이고 한 직선 위에 있지 않을 때 세 점을 지나는 원.
    std::optional<Circle> circle() const noexcept;

    /// 흰 배경 → 테두리 → 정원 → 클릭 지점 원 순서로 그린다 (점이 정원 위에 보이도록).
    void render(GrayCanvas& canvas, const DrawSettings& settings) const noexcept;

private:
    std::vector<Point2D> m_points;
};

} // namespace circle
