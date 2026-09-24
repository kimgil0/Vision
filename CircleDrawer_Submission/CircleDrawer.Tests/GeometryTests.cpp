/**
 * @file GeometryTests.cpp
 * @brief 외접원 계산: 정확도, 순서 무관성, 일직선·중복 점, 큰 좌표.
 */

#include "TestFramework.h"

#include "Geometry.h"

#include <cmath>
#include <random>

using circle::Point2D;

namespace {
bool near(double a, double b, double tolerance) {
    return std::abs(a - b) <= tolerance;
}
} // anonymous namespace

TEST_CASE(Circumcircle_RightTriangleHasCenterAtHypotenuseMidpoint) {
    const auto c = circle::circumcircle({ 0, 0 }, { 4, 0 }, { 0, 3 });
    CHECK(c.has_value());
    CHECK(near(c->center.x, 2.0, 1e-12));
    CHECK(near(c->center.y, 1.5, 1e-12));
    CHECK(near(c->radius, 2.5, 1e-12));
}

TEST_CASE(Circumcircle_PassesThroughAllThreePoints) {
    std::mt19937 rng(7);
    std::uniform_real_distribution<double> coord(0.0, 800.0);
    int checked = 0;
    for (int i = 0; i < 2000; ++i) {
        const Point2D a{ coord(rng), coord(rng) }, b{ coord(rng), coord(rng) }, p{ coord(rng), coord(rng) };
        const auto c = circle::circumcircle(a, b, p);
        if (!c) {
            continue;
        }
        ++checked;
        const double tolerance = 1e-9 * std::max(1.0, c->radius);
        CHECK(near(circle::distance(c->center, a), c->radius, tolerance));
        CHECK(near(circle::distance(c->center, b), c->radius, tolerance));
        CHECK(near(circle::distance(c->center, p), c->radius, tolerance));
    }
    CHECK(checked > 1900);
}

TEST_CASE(Circumcircle_DoesNotDependOnPointOrder) {
    const Point2D a{ 120, 80 }, b{ 300, 60 }, c{ 250, 330 };
    const auto r1 = circle::circumcircle(a, b, c);
    const auto r2 = circle::circumcircle(c, a, b);
    const auto r3 = circle::circumcircle(b, c, a);
    CHECK(r1 && r2 && r3);
    CHECK(near(r1->center.x, r2->center.x, 1e-9) && near(r1->center.x, r3->center.x, 1e-9));
    CHECK(near(r1->center.y, r2->center.y, 1e-9) && near(r1->center.y, r3->center.y, 1e-9));
}

TEST_CASE(Circumcircle_RejectsCollinearAndDuplicatePoints) {
    CHECK(!circle::circumcircle({ 0, 0 }, { 10, 10 }, { 25, 25 }));   // 일직선
    CHECK(!circle::circumcircle({ 5, 5 }, { 5, 5 }, { 100, 7 }));     // 두 점 겹침
    CHECK(!circle::circumcircle({ 9, 9 }, { 9, 9 }, { 9, 9 }));       // 세 점 겹침
    CHECK(!circle::circumcircle({ 0, 100 }, { 200, 100 }, { 400, 100 }));  // 수평선
}

TEST_CASE(Circumcircle_NearlyCollinearIntegerPointsGiveLargeButFiniteCircle) {
    // 정수 픽셀 좌표에서 가장 "납작한" 경우: 원은 존재하지만 반지름이 매우 크다.
    const auto c = circle::circumcircle({ 0, 0 }, { 300, 1 }, { 600, 0 });
    CHECK(c.has_value());
    CHECK(c->radius > 10000.0 && std::isfinite(c->radius));
}

TEST_CASE(Circumcircle_StaysAccurateFarFromOrigin) {
    // 원점에서 먼 작은 삼각형: a 기준 평행이동 덕분에 정밀도가 유지된다.
    const Point2D a{ 1e7 + 0, 1e7 + 0 }, b{ 1e7 + 4, 1e7 + 0 }, c{ 1e7 + 0, 1e7 + 3 };
    const auto r = circle::circumcircle(a, b, c);
    CHECK(r.has_value());
    CHECK(near(r->radius, 2.5, 1e-6));
    CHECK(near(r->center.x, 1e7 + 2.0, 1e-6));
}
