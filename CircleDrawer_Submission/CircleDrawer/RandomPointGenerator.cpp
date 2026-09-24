/**
 * @file RandomPointGenerator.cpp
 */

#include "RandomPointGenerator.h"

#include <algorithm>
#include <cmath>

namespace circle {

namespace {

constexpr int MAX_ATTEMPTS = 1000;

/// 삼각형이 "충분히 통통한지": 넓이가 가장 긴 변 기준 정삼각형 넓이의 일정 비율 이상.
/// 이 비율 미만이면 외접원 반지름이 급격히 커진다.
constexpr double MIN_FATNESS = 0.15;

bool isWellShaped(const std::array<Point2D, 3>& p, double minSeparation) noexcept {
    const double ab = distance(p[0], p[1]);
    const double bc = distance(p[1], p[2]);
    const double ca = distance(p[2], p[0]);
    if (std::min({ ab, bc, ca }) < minSeparation) {
        return false;
    }
    const double twiceArea = std::abs((p[1].x - p[0].x) * (p[2].y - p[0].y) -
                                      (p[1].y - p[0].y) * (p[2].x - p[0].x));
    const double longest = std::max({ ab, bc, ca });
    const double equilateralTwiceArea = std::sqrt(3.0) / 2.0 * longest * longest;
    return twiceArea >= MIN_FATNESS * equilateralTwiceArea;
}

/// 여백이 캔버스보다 크면 여백 없이 전체 범위를 쓴다.
std::uniform_int_distribution<int> makeRange(int size, int margin) {
    const int lo = std::max(0, margin);
    const int hi = size - 1 - margin;
    return (lo <= hi) ? std::uniform_int_distribution<int>(lo, hi)
                      : std::uniform_int_distribution<int>(0, std::max(0, size - 1));
}

} // anonymous namespace

RandomPointGenerator::RandomPointGenerator(int width, int height, int margin, std::uint32_t seed)
    : m_engine(seed)
    , m_xDist(makeRange(width, margin))
    , m_yDist(makeRange(height, margin))
    , m_minSeparation(2.0 * std::max(margin, 1) + 4.0)  // 클릭 지점 원끼리 겹치지 않게
{
}

Point2D RandomPointGenerator::randomPoint() {
    return { static_cast<double>(m_xDist(m_engine)), static_cast<double>(m_yDist(m_engine)) };
}

std::array<Point2D, 3> RandomPointGenerator::next() {
    std::array<Point2D, 3> candidate{};
    for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
        candidate = { randomPoint(), randomPoint(), randomPoint() };
        if (isWellShaped(candidate, m_minSeparation)) {
            return candidate;
        }
    }
    // 캔버스가 매우 작아 조건을 만족할 수 없는 경우: 원이 존재하기만 하면 그대로 쓴다.
    for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
        candidate = { randomPoint(), randomPoint(), randomPoint() };
        if (circumcircle(candidate[0], candidate[1], candidate[2])) {
            break;
        }
    }
    return candidate;
}

} // namespace circle
