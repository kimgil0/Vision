#pragma once

/**
 * @file Geometry.h
 * @brief 점·원 자료형과 세 점을 지나는 원(외접원) 계산. MFC 에 의존하지 않는다 (단위 테스트 대상).
 */

#include <cmath>
#include <optional>

namespace circle {

/// 캔버스 픽셀 좌표 (x: 오른쪽, y: 아래쪽). 정수 좌표 (x, y) 는 픽셀 (x, y) 의 중심이다.
struct Point2D {
    double x = 0.0;
    double y = 0.0;
};

struct Circle {
    Point2D center;
    double  radius = 0.0;
};

inline double distance(const Point2D& a, const Point2D& b) noexcept {
    return std::hypot(a.x - b.x, a.y - b.y);
}

/**
 * @brief 세 점을 모두 지나는 원(외접원)을 구한다.
 *
 * a 를 원점으로 평행이동한 뒤 계산하여 큰 좌표에서도 수치 오차를 줄인다.
 *   d  = 2 (bx·cy − by·cx)                       (삼각형 넓이 × 4)
 *   ux = (cy·|b|² − by·|c|²) / d,  uy = (bx·|c|² − cx·|b|²) / d
 *
 * @return 세 점이 한 직선 위에 있거나 두 점 이상이 겹치면 std::nullopt (원이 존재하지 않음).
 */
std::optional<Circle> circumcircle(const Point2D& a, const Point2D& b, const Point2D& c) noexcept;

} // namespace circle
