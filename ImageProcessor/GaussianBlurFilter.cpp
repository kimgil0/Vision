/**
 * @file GaussianBlurFilter.cpp
 */

#include "GaussianBlurFilter.h"
#include "Exceptions.h"
#include "Parallel.h"
#include "PixelMath.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ip {

namespace {

constexpr int CH = ImageBuffer::CHANNELS;

/// 정규화된 1D 가우시안 커널. 합산은 double 로 하여 정규화 오차를 줄인다.
std::vector<float> makeGaussianKernel(double sigma) {
    const int radius = std::max(1, static_cast<int>(std::ceil(3.0 * sigma)));
    const std::size_t size = static_cast<std::size_t>(2 * radius + 1);

    std::vector<double> weights(size);
    double sum = 0.0;
    for (int i = -radius; i <= radius; ++i) {
        const double weight = std::exp(-(static_cast<double>(i) * i) / (2.0 * sigma * sigma));
        weights[static_cast<std::size_t>(i + radius)] = weight;
        sum += weight;
    }

    std::vector<float> kernel(size);
    for (std::size_t i = 0; i < size; ++i) {
        kernel[i] = static_cast<float>(weights[i] / sum);
    }
    return kernel;
}

} // anonymous namespace

GaussianBlurFilter::GaussianBlurFilter(double sigma)
    : m_sigma(sigma)
{
    // !(a && b) 형태로 비교해야 NaN 도 거부된다 (NaN 과의 비교는 항상 false).
    if (!(sigma > 0.0 && sigma <= MAX_SIGMA)) {
        throw FilterError("blur: sigma must be in (0, " + formatNumber(MAX_SIGMA) +
                          "] (got " + formatNumber(sigma) + ")");
    }
    m_kernel = makeGaussianKernel(sigma);
}

std::string GaussianBlurFilter::describe() const {
    return "blur(sigma=" + formatNumber(m_sigma) + ")";
}

void GaussianBlurFilter::process(ImageBuffer& image, const FilterContext& context) const {
    const int width     = image.width();
    const int height    = image.height();
    const int rowStride = image.rowStride();
    const int radius    = static_cast<int>(m_kernel.size() / 2);
    const int taps      = static_cast<int>(m_kernel.size());
    const std::size_t costPerRow = static_cast<std::size_t>(rowStride) * m_kernel.size();

    // 1차 결과를 담는 별도 버퍼. 원본을 읽는 도중에 덮어쓰면 이미 블러된 값이 섞인다.
    ImageBuffer horizontal(width, height);

    // ── 1차: 가로 방향 (image → horizontal) ─────────────────────
    parallelForRows(height, costPerRow, context.threadCount, [&](int rowBegin, int rowEnd) {
        // 좌우로 radius 픽셀씩 가장자리를 복제한 행 버퍼 → 내부 루프에서 경계 분기를 제거한다.
        std::vector<std::uint8_t> padded(static_cast<std::size_t>(width + 2 * radius) * CH);

        for (int y = rowBegin; y < rowEnd; ++y) {
            const std::uint8_t* src = image.rowPtr(y);
            const std::uint8_t* lastPixel = src + static_cast<std::size_t>(width - 1) * CH;
            for (int i = 0; i < radius; ++i) {
                std::memcpy(&padded[static_cast<std::size_t>(i) * CH], src, CH);
                std::memcpy(&padded[static_cast<std::size_t>(radius + width + i) * CH], lastPixel, CH);
            }
            std::memcpy(&padded[static_cast<std::size_t>(radius) * CH], src, static_cast<std::size_t>(rowStride));

            std::uint8_t* dst = horizontal.rowPtr(y);
            for (int x = 0; x < width; ++x) {
                const std::uint8_t* window = &padded[static_cast<std::size_t>(x) * CH];
                float blue = 0.0f, green = 0.0f, red = 0.0f;
                for (int k = 0; k < taps; ++k) {
                    const float weight = m_kernel[static_cast<std::size_t>(k)];
                    blue  += weight * window[k * CH + pixel::BLUE];
                    green += weight * window[k * CH + pixel::GREEN];
                    red   += weight * window[k * CH + pixel::RED];
                }
                dst[x * CH + pixel::BLUE]  = pixel::saturate(blue);
                dst[x * CH + pixel::GREEN] = pixel::saturate(green);
                dst[x * CH + pixel::RED]   = pixel::saturate(red);
            }
        }
    });

    // ── 2차: 세로 방향 (horizontal → image) ─────────────────────
    // 행 전체를 누적 버퍼에 더해 나가므로 메모리 접근이 연속적이다 (캐시/벡터화 친화적).
    parallelForRows(height, costPerRow, context.threadCount, [&](int rowBegin, int rowEnd) {
        std::vector<float> accumulator(static_cast<std::size_t>(rowStride));

        for (int y = rowBegin; y < rowEnd; ++y) {
            std::fill(accumulator.begin(), accumulator.end(), 0.0f);
            for (int k = -radius; k <= radius; ++k) {
                const int srcY = std::clamp(y + k, 0, height - 1);
                const std::uint8_t* src = horizontal.rowPtr(srcY);
                const float weight = m_kernel[static_cast<std::size_t>(k + radius)];
                for (int i = 0; i < rowStride; ++i) {
                    accumulator[static_cast<std::size_t>(i)] += weight * src[i];
                }
            }

            std::uint8_t* dst = image.rowPtr(y);
            for (int i = 0; i < rowStride; ++i) {
                dst[i] = pixel::saturate(accumulator[static_cast<std::size_t>(i)]);
            }
        }
    });
}

} // namespace ip
