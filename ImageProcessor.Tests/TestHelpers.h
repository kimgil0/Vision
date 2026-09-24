#pragma once

/**
 * @file TestHelpers.h
 * @brief 테스트용 이미지 생성/비교, 임시 파일, BMP 바이트 조립, CLI 파싱 헬퍼.
 */

#include "CommandLineParser.h"
#include "ImageBuffer.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <system_error>
#include <vector>

namespace test {

constexpr int CH = ip::ImageBuffer::CHANNELS;

inline ip::ImageBuffer makeSolid(int width, int height,
                                 std::uint8_t blue, std::uint8_t green, std::uint8_t red) {
    ip::ImageBuffer image(width, height);
    for (int y = 0; y < height; ++y) {
        std::uint8_t* px = image.rowPtr(y);
        for (int x = 0; x < width; ++x, px += CH) {
            px[0] = blue;
            px[1] = green;
            px[2] = red;
        }
    }
    return image;
}

inline ip::ImageBuffer makeGray(int width, int height, std::uint8_t value) {
    return makeSolid(width, height, value, value, value);
}

inline ip::ImageBuffer makeRandom(int width, int height, unsigned seed) {
    ip::ImageBuffer image(width, height);
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(0, 255);
    for (std::size_t i = 0; i < image.dataSize(); ++i) {
        image.data()[i] = static_cast<std::uint8_t>(dist(rng));
    }
    return image;
}

inline void setGray(ip::ImageBuffer& image, int x, int y, std::uint8_t value) {
    std::uint8_t* px = image.rowPtr(y) + x * CH;
    px[0] = px[1] = px[2] = value;
}

inline std::uint8_t pixelAt(const ip::ImageBuffer& image, int x, int y, int channel) {
    return image.rowPtr(y)[x * CH + channel];
}

inline bool sameImage(const ip::ImageBuffer& a, const ip::ImageBuffer& b) {
    return a.width() == b.width() && a.height() == b.height() &&
           std::equal(a.data(), a.data() + a.dataSize(), b.data());
}

/// 테스트 전용 임시 파일. 소멸 시 삭제한다 (RAII).
class TempFile {
public:
    explicit TempFile(const std::string& name)
        : m_path((std::filesystem::temp_directory_path() / ("ip_test_" + name)).string()) {}

    ~TempFile() {
        std::error_code ignored;
        std::filesystem::remove(m_path, ignored);
    }

    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;

    const std::string& path() const noexcept { return m_path; }

    void write(const std::vector<std::uint8_t>& bytes) const {
        std::ofstream file(m_path, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
    }

private:
    std::string m_path;
};

/// 손상/경계 케이스를 재현하기 위해 BMP 헤더를 직접 조립한다.
struct BmpHeaderSpec {
    std::int32_t  width       = 1;
    std::int32_t  height      = 1;
    std::uint16_t bitCount    = 24;
    std::uint32_t compression = 0;
    std::uint32_t infoSize    = 40;
    std::uint32_t pixelOffset = 54;
    std::size_t   pixelBytes  = 0;   ///< 헤더 뒤에 0 으로 채워 붙일 픽셀 영역 크기
};

inline void putLittleEndian(std::vector<std::uint8_t>& bytes, std::size_t offset,
                            std::uint32_t value, int size) {
    for (int i = 0; i < size; ++i) {
        bytes[offset + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>((value >> (8 * i)) & 0xFFu);
    }
}

inline std::vector<std::uint8_t> makeBmpBytes(const BmpHeaderSpec& spec) {
    std::vector<std::uint8_t> bytes(54 + spec.pixelBytes, 0);
    bytes[0] = 'B';
    bytes[1] = 'M';
    putLittleEndian(bytes,  2, static_cast<std::uint32_t>(bytes.size()), 4);
    putLittleEndian(bytes, 10, spec.pixelOffset, 4);
    putLittleEndian(bytes, 14, spec.infoSize, 4);
    putLittleEndian(bytes, 18, static_cast<std::uint32_t>(spec.width), 4);
    putLittleEndian(bytes, 22, static_cast<std::uint32_t>(spec.height), 4);
    putLittleEndian(bytes, 26, 1, 2);
    putLittleEndian(bytes, 28, spec.bitCount, 2);
    putLittleEndian(bytes, 30, spec.compression, 4);
    return bytes;
}

/// {"-i", "a.bmp", ...} → CommandLineParser::parse. argv[0] 은 자동으로 채운다.
inline ip::ProgramOptions parseArgs(std::vector<std::string> args) {
    std::string exeName = "ImageProcessor";
    std::vector<char*> argv;
    argv.push_back(exeName.data());
    for (std::string& arg : args) {
        argv.push_back(arg.data());
    }
    return ip::CommandLineParser::parse(static_cast<int>(argv.size()), argv.data());
}

} // namespace test
