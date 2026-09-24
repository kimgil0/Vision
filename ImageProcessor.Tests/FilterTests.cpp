/**
 * @file FilterTests.cpp
 * @brief 각 필터의 수치적 정확성, 경계 조건, 스레드 수 무관성.
 */

#include "TestFramework.h"
#include "TestHelpers.h"

#include "Exceptions.h"
#include "FilterFactory.h"
#include "GaussianBlurFilter.h"
#include "GrayscaleFilter.h"
#include "InvertFilter.h"
#include "SharpenFilter.h"
#include "SobelFilter.h"
#include "ThresholdFilter.h"

#include <cmath>
#include <limits>
#include <utility>

using namespace test;

// ── Grayscale ───────────────────────────────────────────

TEST_CASE(Grayscale_UsesBt601WeightsOnBgrLayout) {
    struct Case { std::uint8_t blue, green, red; int expected; };
    const Case cases[] = {
        {   0,   0, 255,  77 },  // 순수 빨강: 0.299 · 255
        {   0, 255,   0, 149 },  // 순수 초록: 0.587 · 255
        { 255,   0,   0,  29 },  // 순수 파랑: 0.114 · 255  (RGB/BGR 을 혼동하면 약 76 이 된다)
        { 255, 255, 255, 255 },  // 흰색은 정확히 255 로 보존
        {   0,   0,   0,   0 },
    };
    for (const Case& c : cases) {
        ip::ImageBuffer image = makeSolid(1, 1, c.blue, c.green, c.red);
        ip::GrayscaleFilter().apply(image);
        for (int channel = 0; channel < CH; ++channel) {
            CHECK_EQ(pixelAt(image, 0, 0, channel), c.expected);
        }
    }
}

// ── Invert ──────────────────────────────────────────────

TEST_CASE(Invert_AppliedTwiceIsIdentity) {
    const ip::ImageBuffer original = makeRandom(37, 41, 7);
    ip::ImageBuffer image = original;
    const ip::InvertFilter invert{};

    invert.apply(image);
    CHECK_EQ(pixelAt(image, 3, 5, 1), 255 - pixelAt(original, 3, 5, 1));
    invert.apply(image);
    CHECK(sameImage(image, original));
}

// ── Threshold ───────────────────────────────────────────

TEST_CASE(Threshold_UsesStrictlyGreaterComparison) {
    ip::ImageBuffer image = makeGray(2, 1, 128);
    setGray(image, 1, 0, 129);

    ip::ThresholdFilter(128).apply(image);

    CHECK_EQ(pixelAt(image, 0, 0, 0), 0);    // 128 > 128 거짓 → 검정
    CHECK_EQ(pixelAt(image, 1, 0, 0), 255);  // 129 > 128 참   → 흰색
}

TEST_CASE(Threshold_RejectsOutOfRangeLevel) {
    CHECK_THROWS_AS(ip::ThresholdFilter(-1), ip::FilterError);
    CHECK_THROWS_AS(ip::ThresholdFilter(256), ip::FilterError);
}

TEST_CASE(Threshold_OtsuSeparatesBimodalImage) {
    ip::ImageBuffer image = makeGray(40, 40, 50);
    for (int y = 0; y < 40; ++y) {
        for (int x = 20; x < 40; ++x) {
            setGray(image, x, y, 200);
        }
    }

    const int level = ip::ThresholdFilter::computeOtsuLevel(image);
    CHECK(level >= 50 && level < 200);

    ip::ThresholdFilter(std::nullopt).apply(image);
    CHECK_EQ(pixelAt(image, 5, 5, 0), 0);
    CHECK_EQ(pixelAt(image, 30, 5, 0), 255);
}

// ── Gaussian blur ───────────────────────────────────────

TEST_CASE(Blur_PreservesConstantImage) {
    // 커널이 정규화되어 있고 경계를 복제하므로 균일 영역(테두리 포함)은 변하지 않아야 한다.
    for (const double sigma : { 0.5, 1.0, 3.7, 50.0 }) {
        ip::ImageBuffer image = makeSolid(23, 17, 10, 128, 250);
        const ip::ImageBuffer original = image;
        ip::GaussianBlurFilter(sigma).apply(image);
        CHECK_MSG(sameImage(image, original), "sigma=" + std::to_string(sigma));
    }
}

TEST_CASE(Blur_SpreadsImpulseSymmetrically) {
    ip::ImageBuffer image = makeGray(9, 9, 0);
    setGray(image, 4, 4, 255);

    ip::GaussianBlurFilter(1.0).apply(image);

    const int center = pixelAt(image, 4, 4, 0);
    CHECK(center > 0 && center < 255);
    CHECK(pixelAt(image, 3, 4, 0) > 0);
    CHECK_EQ(pixelAt(image, 3, 4, 0), pixelAt(image, 5, 4, 0));  // 좌우 대칭
    CHECK_EQ(pixelAt(image, 4, 3, 0), pixelAt(image, 4, 5, 0));  // 상하 대칭
}

TEST_CASE(Blur_HandlesImagesSmallerThanKernel) {
    for (const auto& [width, height] : { std::pair{ 1, 1 }, std::pair{ 1, 9 }, std::pair{ 9, 1 } }) {
        ip::ImageBuffer image = makeGray(width, height, 77);
        ip::GaussianBlurFilter(5.0).apply(image);  // 반경 15 > 이미지 크기
        CHECK(sameImage(image, makeGray(width, height, 77)));
    }
}

TEST_CASE(Blur_RejectsInvalidSigma) {
    for (const double sigma : { 0.0, -1.0, 50.5, std::nan(""), std::numeric_limits<double>::infinity() }) {
        CHECK_THROWS_AS(ip::GaussianBlurFilter(sigma), ip::FilterError);
    }
}

// ── Sharpen ─────────────────────────────────────────────

TEST_CASE(Sharpen_KeepsFlatAreasAndBoostsEdgeContrast) {
    ip::ImageBuffer flat = makeGray(16, 16, 100);
    ip::SharpenFilter(2.0).apply(flat);
    CHECK(sameImage(flat, makeGray(16, 16, 100)));

    ip::ImageBuffer edge = makeGray(16, 4, 100);
    for (int y = 0; y < 4; ++y) {
        for (int x = 8; x < 16; ++x) {
            setGray(edge, x, y, 150);
        }
    }
    ip::SharpenFilter(1.0).apply(edge);
    CHECK(pixelAt(edge, 7, 1, 0) < 100);  // 어두운 쪽 경계는 더 어둡게
    CHECK(pixelAt(edge, 8, 1, 0) > 150);  // 밝은 쪽 경계는 더 밝게
}

// ── Sobel ───────────────────────────────────────────────

TEST_CASE(Sobel_FlatImageHasNoEdges) {
    ip::ImageBuffer image = makeSolid(10, 10, 30, 60, 90);
    ip::SobelFilter().apply(image);
    CHECK(sameImage(image, makeGray(10, 10, 0)));
}

TEST_CASE(Sobel_DetectsVerticalEdgeWithoutBorderArtifacts) {
    ip::ImageBuffer image = makeGray(10, 6, 0);
    for (int y = 0; y < 6; ++y) {
        for (int x = 5; x < 10; ++x) {
            setGray(image, x, y, 255);
        }
    }
    ip::SobelFilter().apply(image);

    CHECK_EQ(pixelAt(image, 0, 3, 0), 0);    // 이미지 테두리: 가짜 엣지 없음
    CHECK_EQ(pixelAt(image, 9, 3, 0), 0);
    CHECK_EQ(pixelAt(image, 4, 3, 0), 255);  // 실제 경계
    CHECK_EQ(pixelAt(image, 5, 3, 0), 255);
    CHECK_EQ(pixelAt(image, 4, 0, 0), 255);  // 위쪽 테두리 행에서도 동일하게 검출
}

// ── 공통 계약 ───────────────────────────────────────────

TEST_CASE(Filters_ProduceIdenticalOutputForAnyThreadCount) {
    // 홀수 크기로 블록 경계가 나누어떨어지지 않게 하여 경계 처리 오류를 드러낸다.
    // (점 연산 필터도 실제로 3개 이상 블록으로 나뉘도록 전체 작업량 > 3 × MIN_COST_PER_TASK)
    const ip::ImageBuffer source = makeRandom(1031, 2053, 2026);
    for (const char* spec : { "grayscale", "invert", "threshold:otsu", "blur:2.5", "sharpen:1.5", "sobel" }) {
        const auto filter = ip::FilterFactory::create(spec);
        ip::ImageBuffer single = source;
        ip::ImageBuffer multi  = source;
        filter->apply(single, ip::FilterContext{ 1, nullptr });
        filter->apply(multi,  ip::FilterContext{ 7, nullptr });
        CHECK_MSG(sameImage(single, multi), std::string("filter=") + spec);
    }
}

TEST_CASE(Filters_RejectEmptyImageAndZeroThreads) {
    ip::ImageBuffer empty;
    CHECK_THROWS_AS(ip::GrayscaleFilter().apply(empty), ip::FilterError);
    CHECK_THROWS_AS(ip::FilterFactory::createPipeline("blur, sobel")->apply(empty), ip::FilterError);

    ip::ImageBuffer image = makeGray(4, 4, 0);
    CHECK_THROWS_AS(ip::InvertFilter().apply(image, ip::FilterContext{ 0, nullptr }), ip::FilterError);
}
