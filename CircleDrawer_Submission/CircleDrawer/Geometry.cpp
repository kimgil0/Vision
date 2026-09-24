/**
 * @file Geometry.cpp
 */

#include "Geometry.h"

namespace circle {

namespace {
    /// 일직선 판정 허용 오차 (좌표 크기에 대한 상대값). 정수 픽셀 좌표의 완전한 일직선은 d == 0 이다.
    constexpr double COLLINEAR_EPSILON = 1e-12;
} // anonymous namespace

std::optional<Circle> circumcircle(const Point2D& a, const Point2D& b, const Point2D& c) noexcept {
    const double bx = b.x - a.x;
    const double by = b.y - a.y;
    const double cx = c.x - a.x;
    const double cy = c.y - a.y;

    const double b2 = bx * bx + by * by;
    const double c2 = cx * cx + cy * cy;
    const double d  = 2.0 * (bx * cy - by * cx);

    // 좌표 크기에 비례하는 기준으로 비교해야 척도(scale)와 무관하게 판정된다.
    if (!(std::abs(d) > COLLINEAR_EPSILON * (b2 + c2)) || !std::isfinite(d)) {
        return std::nullopt;
    }

    const double ux = (cy * b2 - by * c2) / d;
    const double uy = (bx * c2 - cx * b2) / d;

    Circle result;
    result.center = { a.x + ux, a.y + uy };
    result.radius = std::hypot(ux, uy);
    return result;
}

} // namespace circle
