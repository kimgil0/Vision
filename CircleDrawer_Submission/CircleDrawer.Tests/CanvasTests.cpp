/**
 * @file CanvasTests.cpp
 * @brief 픽셀 버퍼 직접 그리기: 채운 원, 속 빈 원(두께), 잘림(clipping), 행 패딩 보존.
 */

#include "TestFramework.h"

#include "GrayCanvas.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

using circle::GrayCanvas;
using circle::Point2D;

namespace {

/// CImage 처럼 행 길이를 4바이트 배수로 맞춘 버퍼 + 캔버스 뷰.
struct TestCanvas {
    int width, height, pitch;
    std::vector<std::uint8_t> buffer;
    GrayCanvas canvas;

    TestCanvas(int w, int h)
        : width(w), height(h), pitch((w + 3) / 4 * 4),
          buffer(static_cast<std::size_t>((w + 3) / 4 * 4) * h, 0x77),
          canvas(buffer.data(), w, h, (w + 3) / 4 * 4) {
        canvas.fill(GrayCanvas::WHITE);
    }

    int darkPixels(int threshold = 128) const {
        int count = 0;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                if (canvas.pixel(x, y) < threshold) ++count;
        return count;
    }
};

} // anonymous namespace

TEST_CASE(Canvas_RejectsInvalidBuffer) {
    std::vector<std::uint8_t> buffer(64);
    CHECK_THROWS_AS(GrayCanvas(nullptr, 4, 4, 4), std::invalid_argument);
    CHECK_THROWS_AS(GrayCanvas(buffer.data(), 0, 4, 4), std::invalid_argument);
    CHECK_THROWS_AS(GrayCanvas(buffer.data(), 8, 4, 4), std::invalid_argument);  // pitch < width
    TestCanvas t(4, 4);
    CHECK_THROWS_AS(t.canvas.pixel(4, 0), std::out_of_range);
}

TEST_CASE(Canvas_FillDiskCoversCircleArea) {
    TestCanvas t(101, 101);
    t.canvas.fillDisk({ 50, 50 }, 20, GrayCanvas::BLACK);

    CHECK_EQ(t.canvas.pixel(50, 50), 0);        // 중심
    CHECK_EQ(t.canvas.pixel(50 + 18, 50), 0);   // 반지름 안
    CHECK_EQ(t.canvas.pixel(50 + 23, 50), 255); // 반지름 밖
    // 칠해진 넓이 ≈ πr² (안티에일리어싱 경계 포함 ±3%)
    const double area = 3.14159265358979 * 20 * 20;
    CHECK(std::abs(t.darkPixels() - area) < area * 0.03);
}

TEST_CASE(Canvas_StrokeCircleDrawsRingOfRequestedThicknessOnly) {
    TestCanvas t(201, 201);
    t.canvas.strokeCircle({ 100, 100 }, 60, 5, GrayCanvas::BLACK);

    CHECK_EQ(t.canvas.pixel(100, 100), 255);        // 원 내부는 채우지 않음
    CHECK_EQ(t.canvas.pixel(160, 100), 0);          // 둘레 위
    CHECK_EQ(t.canvas.pixel(100, 40), 0);
    CHECK_EQ(t.canvas.pixel(100 + 70, 100), 255);   // 바깥

    // 수평선 위에서 어두운 픽셀 수 = 두께
    int dark = 0;
    for (int x = 140; x < 180; ++x) {
        if (t.canvas.pixel(x, 100) < 128) ++dark;
    }
    CHECK_EQ(dark, 5);
}

TEST_CASE(Canvas_ClipsShapesCrossingTheEdge) {
    TestCanvas t(50, 40);
    t.canvas.fillDisk({ -5, 20 }, 12, GrayCanvas::BLACK);          // 왼쪽 밖으로 걸친 점
    t.canvas.strokeCircle({ 25, 200 }, 175, 3, GrayCanvas::BLACK);  // 아래쪽 밖에 중심이 있는 원
    t.canvas.fillDisk({ 5000, 5000 }, 10, GrayCanvas::BLACK);      // 완전히 밖
    CHECK(t.canvas.pixel(0, 20) < 128);
    CHECK(t.canvas.pixel(25, 25) < 128);  // 원의 위쪽 호가 캔버스를 지남
}

TEST_CASE(Canvas_HugeRadiusCircleIsFastAndBounded) {
    // 세 점이 거의 일직선이면 반지름이 수천만 px 가 될 수 있다. 캔버스 크기만큼만 계산해야 한다.
    TestCanvas t(640, 480);
    const auto start = std::chrono::steady_clock::now();
    t.canvas.strokeCircle({ 320, 240 + 5e7 }, 5e7, 3, GrayCanvas::BLACK);  // y≈240 의 거의 수평선
    const auto elapsed = std::chrono::steady_clock::now() - start;
    CHECK(elapsed < std::chrono::milliseconds(200));
    CHECK(t.canvas.pixel(10, 240) < 128);
    CHECK(t.canvas.pixel(630, 240) < 128);
    CHECK_EQ(t.canvas.pixel(320, 100), 255);
}

TEST_CASE(Canvas_IgnoresInvalidParametersWithoutCrashing) {
    TestCanvas t(20, 20);
    t.canvas.fillDisk({ 10, 10 }, 0, GrayCanvas::BLACK);
    t.canvas.fillDisk({ 10, 10 }, -3, GrayCanvas::BLACK);
    t.canvas.strokeCircle({ 10, 10 }, 5, 0, GrayCanvas::BLACK);
    t.canvas.strokeCircle({ std::nan(""), 10 }, 5, 2, GrayCanvas::BLACK);
    t.canvas.strokeCircle({ 10, 10 }, std::numeric_limits<double>::infinity(), 2, GrayCanvas::BLACK);
    CHECK_EQ(t.darkPixels(), 0);
}

TEST_CASE(Canvas_NeverWritesIntoRowPadding) {
    // 폭 5 → pitch 8: 각 행 끝 3바이트는 패딩. CImage 버퍼와 같은 구조에서 범위 밖 쓰기가 없어야 한다.
    TestCanvas t(5, 5);
    for (int y = 0; y < 5; ++y)
        for (int x = 5; x < 8; ++x)
            t.buffer[static_cast<std::size_t>(y) * 8 + x] = 0x77;
    t.canvas.fill(GrayCanvas::WHITE);
    t.canvas.fillDisk({ 4, 2 }, 6, GrayCanvas::BLACK);
    t.canvas.strokeCircle({ 2, 2 }, 3, 3, GrayCanvas::BLACK);
    for (int y = 0; y < 5; ++y)
        for (int x = 5; x < 8; ++x)
            CHECK_EQ(t.buffer[static_cast<std::size_t>(y) * 8 + x], 0x77);
}
