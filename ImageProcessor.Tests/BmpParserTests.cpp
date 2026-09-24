/**
 * @file BmpParserTests.cpp
 * @brief BMP 입출력: 패딩, 방향(bottom-up/top-down), 손상 파일 방어.
 */

#include "TestFramework.h"
#include "TestHelpers.h"

#include "BmpParser.h"
#include "Exceptions.h"

#include <cstdint>
#include <filesystem>
#include <limits>

using namespace test;

TEST_CASE(Bmp_RoundTrip_OddWidthPreservesEveryPixel) {
    // 5px × 3B = 15B → 행마다 1바이트 패딩. 3_chelsea_cat.bmp(451px)와 같은 상황.
    const ip::ImageBuffer original = makeRandom(5, 3, 42);
    const TempFile file("roundtrip_odd.bmp");

    ip::BmpParser::saveToFile(file.path(), original);
    const ip::ImageBuffer loaded = ip::BmpParser::loadFromFile(file.path());

    CHECK(sameImage(original, loaded));
    CHECK_EQ(std::filesystem::file_size(file.path()), std::uintmax_t{ 54 + 16 * 3 });
}

TEST_CASE(Bmp_LoadsBottomUpAndTopDownInCorrectOrientation) {
    for (const bool topDown : { false, true }) {
        BmpHeaderSpec spec;
        spec.width      = 1;
        spec.height     = topDown ? -2 : 2;
        spec.pixelBytes = 8;  // stride 4 × 2 행
        std::vector<std::uint8_t> bytes = makeBmpBytes(spec);
        bytes[54]     = 10;   // 파일상 첫 번째 행
        bytes[54 + 4] = 20;   // 파일상 두 번째 행

        const TempFile file(topDown ? "top_down.bmp" : "bottom_up.bmp");
        file.write(bytes);
        const ip::ImageBuffer image = ip::BmpParser::loadFromFile(file.path());

        // top-down 은 파일 첫 행이 이미지 맨 위(y=0), bottom-up 은 맨 아래(y=1).
        CHECK_EQ(pixelAt(image, 0, topDown ? 0 : 1, 0), 10);
        CHECK_EQ(pixelAt(image, 0, topDown ? 1 : 0, 0), 20);
    }
}

TEST_CASE(Bmp_RejectsHeaderClaimingMoreDataThanFileHas) {
    // 54바이트 파일이 20000×20000 (≈1.2GB) 을 주장 → 할당 전에 BmpParseError 로 거부.
    BmpHeaderSpec spec;
    spec.width  = 20000;
    spec.height = 20000;
    const TempFile file("truncated.bmp");
    file.write(makeBmpBytes(spec));

    CHECK_THROWS_CONTAINS(ip::BmpParser::loadFromFile(file.path()), ip::BmpParseError, "truncated");
}

TEST_CASE(Bmp_RejectsIntMinHeightWithoutUndefinedBehavior) {
    BmpHeaderSpec spec;
    spec.height = std::numeric_limits<std::int32_t>::min();
    const TempFile file("int_min_height.bmp");
    file.write(makeBmpBytes(spec));

    CHECK_THROWS_AS(ip::BmpParser::loadFromFile(file.path()), ip::BmpParseError);
}

TEST_CASE(Bmp_RejectsPixelOffsetInsideHeader) {
    BmpHeaderSpec spec;
    spec.pixelOffset = 20;
    spec.pixelBytes  = 4;
    const TempFile file("bad_offset.bmp");
    file.write(makeBmpBytes(spec));

    CHECK_THROWS_CONTAINS(ip::BmpParser::loadFromFile(file.path()), ip::BmpParseError, "offset");
}

TEST_CASE(Bmp_RejectsUnsupportedFormats) {
    BmpHeaderSpec eightBit;
    eightBit.bitCount = 8;
    BmpHeaderSpec compressed;
    compressed.compression = 1;
    BmpHeaderSpec coreHeader;
    coreHeader.infoSize = 12;

    for (const BmpHeaderSpec& spec : { eightBit, compressed, coreHeader }) {
        BmpHeaderSpec withPixels = spec;
        withPixels.pixelBytes = 4;
        const TempFile file("unsupported.bmp");
        file.write(makeBmpBytes(withPixels));
        CHECK_THROWS_AS(ip::BmpParser::loadFromFile(file.path()), ip::BmpParseError);
    }
}

TEST_CASE(Bmp_RejectsMissingAndNonBmpFiles) {
    const TempFile missing("does_not_exist.bmp");
    CHECK_THROWS_AS(ip::BmpParser::loadFromFile(missing.path()), ip::BmpParseError);

    const TempFile notBmp("not_a_bmp.bmp");
    notBmp.write({ 'h', 'e', 'l', 'l', 'o' });
    CHECK_THROWS_AS(ip::BmpParser::loadFromFile(notBmp.path()), ip::BmpParseError);
}

TEST_CASE(Bmp_RejectsSavingEmptyImage) {
    const TempFile file("empty.bmp");
    CHECK_THROWS_AS(ip::BmpParser::saveToFile(file.path(), ip::ImageBuffer{}), ip::BmpParseError);
}
