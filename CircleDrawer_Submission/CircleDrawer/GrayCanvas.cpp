/**
 * @file GrayCanvas.cpp
 */

#include "GrayCanvas.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>

namespace circle {

namespace {

/// 픽셀 중심에서 원점까지 거리가 dist 일 때, 픽셀의 반경 방향 폭 [dist − 0.5, dist + 0.5] 가
/// 구간 [inner, outer] 와 겹치는 비율. 1차원 근사로 충분히 부드러운 가장자리를 얻는다.
double radialCoverage(double dist, double inner, double outer) noexcept {
    const double overlap = std::min(outer, dist + 0.5) - std::max(inner, dist - 0.5);
    return std::clamp(overlap, 0.0, 1.0);
}

/// 실수 좌표를 [lo, hi] 로 자른 뒤 정수로 변환한다. 거대한 값(반지름 1e9 등)의 int 변환 오버플로를 막는다.
int clampToInt(double value, int lo, int hi) noexcept {
    return static_cast<int>(std::clamp(value, static_cast<double>(lo), static_cast<double>(hi)));
}

} // anonymous namespace

GrayCanvas::GrayCanvas(std::uint8_t* bits, int width, int height, int pitch)
    : m_bits(bits), m_width(width), m_height(height), m_pitch(pitch)
{
    if (bits == nullptr || width <= 0 || height <= 0 || pitch < width) {
        throw std::invalid_argument("GrayCanvas: invalid buffer (" + std::to_string(width) + "x" +
                                    std::to_string(height) + ", pitch " + std::to_string(pitch) + ")");
    }
}

std::uint8_t GrayCanvas::pixel(int x, int y) const {
    if (x < 0 || y < 0 || x >= m_width || y >= m_height) {
        throw std::out_of_range("GrayCanvas::pixel: (" + std::to_string(x) + ", " + std::to_string(y) + ")");
    }
    return m_bits[static_cast<std::size_t>(y) * m_pitch + x];
}

void GrayCanvas::fill(std::uint8_t value) noexcept {
    for (int y = 0; y < m_height; ++y) {
        std::memset(m_bits + static_cast<std::size_t>(y) * m_pitch, value, static_cast<std::size_t>(m_width));
    }
}

void GrayCanvas::drawBorder(std::uint8_t ink) noexcept {
    for (int x = 0; x < m_width; ++x) {
        m_bits[x] = ink;
        m_bits[static_cast<std::size_t>(m_height - 1) * m_pitch + x] = ink;
    }
    for (int y = 0; y < m_height; ++y) {
        m_bits[static_cast<std::size_t>(y) * m_pitch] = ink;
        m_bits[static_cast<std::size_t>(y) * m_pitch + m_width - 1] = ink;
    }
}

void GrayCanvas::blend(int x, int y, double coverage, std::uint8_t ink) noexcept {
    if (coverage <= 0.0) {
        return;
    }
    std::uint8_t& target = m_bits[static_cast<std::size_t>(y) * m_pitch + x];
    const double mixed = target + (static_cast<double>(ink) - target) * coverage;
    const auto value = static_cast<std::uint8_t>(std::lround(std::clamp(mixed, 0.0, 255.0)));
    // 도형이 겹칠 때(점과 정원 등) 먼저 그린 진한 픽셀이 연해지지 않게 한다.
    target = (ink <= target) ? std::min(target, value) : std::max(target, value);
}

void GrayCanvas::fillDisk(const Point2D& center, double radius, std::uint8_t ink) noexcept {
    if (!(radius > 0.0) || !std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(radius)) {
        return;
    }
    const double reach = radius + 1.0;  // 안티에일리어싱 가장자리 포함
    const int x0 = clampToInt(std::floor(center.x - reach), 0, m_width - 1);
    const int x1 = clampToInt(std::ceil(center.x + reach), 0, m_width - 1);
    const int y0 = clampToInt(std::floor(center.y - reach), 0, m_height - 1);
    const int y1 = clampToInt(std::ceil(center.y + reach), 0, m_height - 1);
    if (center.x + reach < 0 || center.x - reach > m_width - 1 ||
        center.y + reach < 0 || center.y - reach > m_height - 1) {
        return;  // 완전히 캔버스 밖
    }

    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const double dist = std::hypot(x - center.x, y - center.y);
            blend(x, y, radialCoverage(dist, -1.0, radius), ink);
        }
    }
}

void GrayCanvas::strokeCircle(const Point2D& center, double radius, double thickness, std::uint8_t ink) noexcept {
    if (!(radius > 0.0) || !(thickness > 0.0) ||
        !std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(radius)) {
        return;
    }
    const double inner = std::max(0.0, radius - thickness / 2.0);
    const double outer = radius + thickness / 2.0;
    const double scanOuter = outer + 1.0;                   // 안티에일리어싱 여유
    const double scanInner = std::max(0.0, inner - 1.0);

    if (center.y + scanOuter < 0 || center.y - scanOuter > m_height - 1) {
        return;  // 고리 전체가 캔버스 위·아래 밖
    }
    const int y0 = clampToInt(std::floor(center.y - scanOuter), 0, m_height - 1);
    const int y1 = clampToInt(std::ceil(center.y + scanOuter), 0, m_height - 1);

    const auto paintSpan = [&](int y, double left, double right) {
        if (right < 0 || left > m_width - 1) {
            return;
        }
        const int xa = clampToInt(std::floor(left), 0, m_width - 1);
        const int xb = clampToInt(std::ceil(right), 0, m_width - 1);
        for (int x = xa; x <= xb; ++x) {
            const double dist = std::hypot(x - center.x, y - center.y);
            blend(x, y, radialCoverage(dist, inner, outer), ink);
        }
    };

    for (int y = y0; y <= y1; ++y) {
        const double dy = y - center.y;
        const double dy2 = dy * dy;
        const double outerSq = scanOuter * scanOuter;
        if (dy2 > outerSq) {
            continue;
        }
        const double halfOuter = std::sqrt(outerSq - dy2);
        const double innerSq = scanInner * scanInner;
        if (dy2 < innerSq) {
            // 이 행은 고리가 좌우 두 구간으로 나뉜다. 가운데(원 내부)는 건너뛴다.
            const double halfInner = std::sqrt(innerSq - dy2);
            paintSpan(y, center.x - halfOuter, center.x - halfInner);
            paintSpan(y, center.x + halfInner, center.x + halfOuter);
        }
        else {
            paintSpan(y, center.x - halfOuter, center.x + halfOuter);  // 원의 위·아래 끝부분
        }
    }
}

} // namespace circle
