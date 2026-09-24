/**
 * @file CircleScene.cpp
 */

#include "CircleScene.h"

#include <limits>
#include <stdexcept>
#include <string>

namespace circle {

namespace {
    constexpr std::uint8_t BORDER_GRAY = 160;
} // anonymous namespace

bool CircleScene::addPoint(const Point2D& point) {
    if (m_points.size() >= MAX_POINTS) {
        return false;
    }
    m_points.push_back(point);
    return true;
}

std::optional<std::size_t> CircleScene::hitTest(const Point2D& point, double grabRadius) const noexcept {
    std::optional<std::size_t> best;
    double bestDistance = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < m_points.size(); ++i) {
        const double d = distance(point, m_points[i]);
        if (d <= grabRadius && d < bestDistance) {
            best = i;
            bestDistance = d;
        }
    }
    return best;
}

void CircleScene::movePoint(std::size_t index, const Point2D& point) {
    if (index >= m_points.size()) {
        throw std::out_of_range("CircleScene::movePoint: index " + std::to_string(index));
    }
    m_points[index] = point;
}

void CircleScene::setPoints(const std::array<Point2D, MAX_POINTS>& points) {
    m_points.assign(points.begin(), points.end());
}

std::optional<Circle> CircleScene::circle() const noexcept {
    if (!isComplete()) {
        return std::nullopt;
    }
    return circumcircle(m_points[0], m_points[1], m_points[2]);
}

void CircleScene::render(GrayCanvas& canvas, const DrawSettings& settings) const noexcept {
    canvas.fill(GrayCanvas::WHITE);
    canvas.drawBorder(BORDER_GRAY);

    if (const auto c = circle()) {
        canvas.strokeCircle(c->center, c->radius, settings.circleThickness, GrayCanvas::BLACK);
    }
    for (const Point2D& p : m_points) {
        canvas.fillDisk(p, settings.pointRadius, GrayCanvas::BLACK);
    }
}

} // namespace circle
