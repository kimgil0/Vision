/**
 * @file BmpParser.cpp
 */

#include "BmpParser.h"
#include "Exceptions.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace ip {

namespace {

// BMP 헤더 구조체 — 디스크상의 바이너리 레이아웃과 동일해야 한다.
// 컴파일러의 패딩을 제거하기 위해 #pragma pack 사용.
#pragma pack(push, 1)

struct BitmapFileHeader {
    std::uint16_t bfType;      // "BM"
    std::uint32_t bfSize;      // 파일 전체 크기
    std::uint16_t bfReserved1;
    std::uint16_t bfReserved2;
    std::uint32_t bfOffBits;   // 픽셀 데이터 시작 오프셋
};

struct BitmapInfoHeader {
    std::uint32_t biSize;          // 정보 헤더 크기 (40)
    std::int32_t  biWidth;
    std::int32_t  biHeight;        // 양수: bottom-up, 음수: top-down
    std::uint16_t biPlanes;
    std::uint16_t biBitCount;      // 24 만 지원
    std::uint32_t biCompression;   // 0 (BI_RGB) 만 지원
    std::uint32_t biSizeImage;
    std::int32_t  biXPelsPerMeter;
    std::int32_t  biYPelsPerMeter;
    std::uint32_t biClrUsed;
    std::uint32_t biClrImportant;
};

#pragma pack(pop)

static_assert(sizeof(BitmapFileHeader) == 14, "BitmapFileHeader must be 14 bytes");
static_assert(sizeof(BitmapInfoHeader) == 40, "BitmapInfoHeader must be 40 bytes");

constexpr std::uint32_t BI_RGB = 0;

} // anonymous namespace

int BmpParser::paddedStride(int width) {
    const int rawStride = width * ImageBuffer::CHANNELS;
    const int remainder = rawStride % ROW_ALIGNMENT;
    return (remainder == 0) ? rawStride : (rawStride + (ROW_ALIGNMENT - remainder));
}

ImageBuffer BmpParser::loadFromFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw BmpParseError("Cannot open file for reading: " + path);
    }

    // [수정] 헤더가 주장하는 크기를 실제 파일 크기와 대조하기 위해 먼저 구해 둔다.
    file.seekg(0, std::ios::end);
    const std::streamoff fileSize = static_cast<std::streamoff>(file.tellg());
    file.seekg(0, std::ios::beg);
    if (!file || fileSize < 0) {
        throw BmpParseError("Cannot determine file size: " + path);
    }

    // ── 1. 파일 헤더 읽기 ───────────────────────────────
    BitmapFileHeader fileHeader{};
    file.read(reinterpret_cast<char*>(&fileHeader), sizeof(fileHeader));
    if (!file) {
        throw BmpParseError("Failed to read BITMAPFILEHEADER from: " + path);
    }
    if (fileHeader.bfType != BMP_MAGIC) {
        throw BmpParseError("Not a BMP file (magic mismatch): " + path);
    }

    // ── 2. 정보 헤더 읽기 ───────────────────────────────
    BitmapInfoHeader infoHeader{};
    file.read(reinterpret_cast<char*>(&infoHeader), sizeof(infoHeader));
    if (!file) {
        throw BmpParseError("Failed to read BITMAPINFOHEADER from: " + path);
    }

    // [수정] BITMAPCOREHEADER(12바이트, OS/2) 처럼 필드 배치가 다른 헤더를
    //        BITMAPINFOHEADER 로 오해석하지 않도록 거부한다.
    //        V4(108)/V5(124) 헤더는 앞 40바이트가 동일하므로 그대로 허용된다.
    if (infoHeader.biSize < sizeof(BitmapInfoHeader)) {
        throw BmpParseError("Unsupported BMP header size: " + std::to_string(infoHeader.biSize));
    }
    if (infoHeader.biBitCount != SUPPORTED_BPP) {
        throw BmpParseError(
            "Unsupported bit depth: " + std::to_string(infoHeader.biBitCount) +
            " (only 24-bit BMP is supported)");
    }
    if (infoHeader.biCompression != BI_RGB) {
        throw BmpParseError("Compressed BMP is not supported (compression=" +
                            std::to_string(infoHeader.biCompression) + ")");
    }
    if (infoHeader.biWidth <= 0) {
        throw BmpParseError("Invalid BMP width: " + std::to_string(infoHeader.biWidth));
    }

    // [수정] INT32_MIN 의 부호 반전은 부호 있는 정수 오버플로(정의되지 않은 동작)이므로 먼저 거부한다.
    if (infoHeader.biHeight == std::numeric_limits<std::int32_t>::min()) {
        throw BmpParseError("Invalid BMP height: " + std::to_string(infoHeader.biHeight));
    }

    const int width = infoHeader.biWidth;
    // 음수 height (top-down) 도 처리할 수 있도록 절댓값으로 변환.
    const bool isTopDown = (infoHeader.biHeight < 0);
    const int height = isTopDown ? -infoHeader.biHeight : infoHeader.biHeight;
    if (height <= 0) {
        throw BmpParseError("Invalid BMP height: " + std::to_string(infoHeader.biHeight));
    }

    // [수정] 메모리를 할당하기 전에 헤더 값의 일관성을 검증한다.
    //   - 픽셀 데이터 오프셋이 헤더 영역을 침범하지 않는지
    //   - 파일에 (stride × height) 바이트가 실제로 존재하는지
    //   기존 코드는 수십 바이트짜리 손상/악성 파일이 헤더에 큰 크기만 적어도
    //   수백 MB 를 먼저 할당한 뒤에야 EOF 로 실패했다. (64비트로 계산해 오버플로 방지)
    const std::uint64_t headerEnd =
        sizeof(BitmapFileHeader) + static_cast<std::uint64_t>(infoHeader.biSize);
    if (fileHeader.bfOffBits < headerEnd) {
        throw BmpParseError("Invalid pixel data offset: " + std::to_string(fileHeader.bfOffBits));
    }
    const std::uint64_t stride64 =
        (static_cast<std::uint64_t>(width) * ImageBuffer::CHANNELS + (ROW_ALIGNMENT - 1)) /
        ROW_ALIGNMENT * ROW_ALIGNMENT;
    const std::uint64_t requiredSize =
        fileHeader.bfOffBits + stride64 * static_cast<std::uint64_t>(height);
    if (requiredSize > static_cast<std::uint64_t>(fileSize)) {
        throw BmpParseError("File is truncated: header requires " + std::to_string(requiredSize) +
                            " bytes but file has " + std::to_string(fileSize) + " (" + path + ")");
    }

    // ── 3. 픽셀 데이터로 이동 ──────────────────────────
    file.seekg(fileHeader.bfOffBits, std::ios::beg);
    if (!file) {
        throw BmpParseError("Failed to seek to pixel data");
    }

    // ── 4. 픽셀 데이터 읽기 ────────────────────────────
    // [수정] 크기 초과 시 ImageBuffer 가 던지는 std::invalid_argument 는 main 에서
    //        "Unexpected error"(종료 코드 1)로 분류되었다. BMP 도메인 오류(2)로 변환한다.
    ImageBuffer image;
    try {
        image = ImageBuffer(width, height);
    }
    catch (const std::invalid_argument& e) {
        throw BmpParseError(std::string("Unsupported image dimensions: ") + e.what());
    }
    const int stride = paddedStride(width);
    std::vector<std::uint8_t> rowBuffer(stride);

    // BMP는 기본적으로 bottom-up: 파일의 첫 행이 이미지의 마지막 행.
    for (int row = 0; row < height; ++row) {
        file.read(reinterpret_cast<char*>(rowBuffer.data()), stride);
        if (!file) {
            throw BmpParseError("Unexpected EOF while reading pixel rows (row=" +
                                std::to_string(row) + ")");
        }
        const int dstY = isTopDown ? row : (height - 1 - row);
        std::memcpy(image.rowPtr(dstY), rowBuffer.data(), image.rowStride());
    }

    return image;
}

void BmpParser::saveToFile(const std::string& path, const ImageBuffer& image) {
    if (image.empty()) {
        throw BmpParseError("Cannot save an empty image to: " + path);
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        throw BmpParseError("Cannot open file for writing: " + path);
    }

    const int width  = image.width();
    const int height = image.height();
    const int stride = paddedStride(width);

    const std::uint32_t pixelDataSize = static_cast<std::uint32_t>(stride) *
                                        static_cast<std::uint32_t>(height);

    // ── 1. 헤더 구성 ────────────────────────────────────
    BitmapFileHeader fileHeader{};
    fileHeader.bfType      = BMP_MAGIC;
    fileHeader.bfOffBits   = sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader);
    fileHeader.bfSize      = fileHeader.bfOffBits + pixelDataSize;

    BitmapInfoHeader infoHeader{};
    infoHeader.biSize        = sizeof(BitmapInfoHeader);
    infoHeader.biWidth       = width;
    infoHeader.biHeight      = height;        // bottom-up 으로 저장
    infoHeader.biPlanes      = 1;
    infoHeader.biBitCount    = SUPPORTED_BPP;
    infoHeader.biCompression = BI_RGB;
    infoHeader.biSizeImage   = pixelDataSize;

    // ── 2. 헤더 쓰기 ────────────────────────────────────
    file.write(reinterpret_cast<const char*>(&fileHeader), sizeof(fileHeader));
    file.write(reinterpret_cast<const char*>(&infoHeader), sizeof(infoHeader));

    // ── 3. 픽셀 데이터 쓰기 (bottom-up) ─────────────────
    std::vector<std::uint8_t> rowBuffer(stride, 0);
    for (int row = 0; row < height; ++row) {
        const int srcY = height - 1 - row;  // bottom-up
        std::memcpy(rowBuffer.data(), image.rowPtr(srcY), image.rowStride());
        // 패딩 영역은 이미 0으로 초기화되어 있음
        file.write(reinterpret_cast<const char*>(rowBuffer.data()), stride);
    }

    if (!file) {
        throw BmpParseError("Failed to write pixel data to: " + path);
    }
}

} // namespace ip
