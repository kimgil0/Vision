/**
 * @file ThresholdFilter.cpp
 */

#include "ThresholdFilter.h"
#include "Exceptions.h"
#include "Logger.h"
#include "Parallel.h"
#include "PixelMath.h"

#include <array>
#include <cstdint>

namespace ip {

namespace {
    constexpr int HISTOGRAM_BINS = 256;
} // anonymous namespace

ThresholdFilter::ThresholdFilter(std::optional<int> level)
    : m_level(level)
{
    if (m_level && (*m_level < MIN_LEVEL || *m_level > MAX_LEVEL)) {
        throw FilterError("threshold: level must be in [" + std::to_string(MIN_LEVEL) + ", " +
                          std::to_string(MAX_LEVEL) + "] (got " + std::to_string(*m_level) + ")");
    }
}

std::string ThresholdFilter::describe() const {
    return m_level ? "threshold(level=" + std::to_string(*m_level) + ")"
                   : std::string("threshold(otsu)");
}

int ThresholdFilter::computeOtsuLevel(const ImageBuffer& image) {
    std::array<std::uint64_t, HISTOGRAM_BINS> histogram{};
    const int width = image.width();
    for (int y = 0; y < image.height(); ++y) {
        const std::uint8_t* px = image.rowPtr(y);
        for (int x = 0; x < width; ++x, px += ImageBuffer::CHANNELS) {
            ++histogram[pixel::luma(px[pixel::BLUE], px[pixel::GREEN], px[pixel::RED])];
        }
    }

    std::uint64_t totalCount = 0;
    std::uint64_t totalSum   = 0;
    for (int level = 0; level < HISTOGRAM_BINS; ++level) {
        totalCount += histogram[level];
        totalSum   += static_cast<std::uint64_t>(level) * histogram[level];
    }

    // 임계값 t 로 [0, t] / (t, 255] 두 클래스로 나눌 때
    //   클래스 간 분산 σ²(t) = w0 · w1 · (μ0 − μ1)²   을 최대화하는 t 를 찾는다.
    std::uint64_t backgroundCount = 0;
    std::uint64_t backgroundSum   = 0;
    double bestVariance = -1.0;
    int bestLevel = 0;
    for (int level = 0; level < HISTOGRAM_BINS; ++level) {
        backgroundCount += histogram[level];
        backgroundSum   += static_cast<std::uint64_t>(level) * histogram[level];
        if (backgroundCount == 0) {
            continue;
        }
        const std::uint64_t foregroundCount = totalCount - backgroundCount;
        if (foregroundCount == 0) {
            break;
        }

        const double w0 = static_cast<double>(backgroundCount);
        const double w1 = static_cast<double>(foregroundCount);
        const double meanDiff = static_cast<double>(backgroundSum) / w0 -
                                static_cast<double>(totalSum - backgroundSum) / w1;
        const double variance = w0 * w1 * meanDiff * meanDiff;
        if (variance > bestVariance) {
            bestVariance = variance;
            bestLevel = level;
        }
    }
    return bestLevel;
}

void ThresholdFilter::process(ImageBuffer& image, const FilterContext& context) const {
    const int level = m_level ? *m_level : computeOtsuLevel(image);
    if (!m_level && context.logger != nullptr) {
        context.logger->info("  threshold: Otsu level = " + std::to_string(level));
    }

    const int width = image.width();
    const std::size_t costPerRow = static_cast<std::size_t>(image.rowStride());
    parallelForRows(image.height(), costPerRow, context.threadCount, [&](int rowBegin, int rowEnd) {
        for (int y = rowBegin; y < rowEnd; ++y) {
            std::uint8_t* px = image.rowPtr(y);
            for (int x = 0; x < width; ++x, px += ImageBuffer::CHANNELS) {
                const std::uint8_t value =
                    pixel::luma(px[pixel::BLUE], px[pixel::GREEN], px[pixel::RED]) > level
                        ? pixel::MAX_VALUE : pixel::MIN_VALUE;
                px[pixel::BLUE]  = value;
                px[pixel::GREEN] = value;
                px[pixel::RED]   = value;
            }
        }
    });
}

} // namespace ip
