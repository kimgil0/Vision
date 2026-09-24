/**
 * @file SobelFilter.cpp
 */

#include "SobelFilter.h"
#include "Parallel.h"
#include "PixelMath.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace ip {

void SobelFilter::process(ImageBuffer& image, const FilterContext& context) const {
    const int width  = image.width();
    const int height = image.height();

    // ── 1. 휘도 평면(1채널) 추출 ─────────────────────────────
    // 결과를 원본에 바로 쓰면 아직 읽지 않은 이웃 픽셀이 오염되므로 별도 버퍼에 둔다.
    std::vector<std::uint8_t> luma(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    const auto lumaRow = [&](int y) {
        return luma.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width);
    };
    const std::size_t lumaCostPerRow     = static_cast<std::size_t>(image.rowStride());
    const std::size_t gradientCostPerRow = static_cast<std::size_t>(width) * 10;  // 실측: 픽셀당 grayscale 의 약 3배

    parallelForRows(height, lumaCostPerRow, context.threadCount, [&](int rowBegin, int rowEnd) {
        for (int y = rowBegin; y < rowEnd; ++y) {
            const std::uint8_t* px = image.rowPtr(y);
            std::uint8_t* dst = lumaRow(y);
            for (int x = 0; x < width; ++x, px += ImageBuffer::CHANNELS) {
                dst[x] = pixel::luma(px[pixel::BLUE], px[pixel::GREEN], px[pixel::RED]);
            }
        }
    });

    // ── 2. 3×3 Sobel 그래디언트 ──────────────────────────────
    //        Gx = [-1 0 1; -2 0 2; -1 0 1],  Gy = [-1 -2 -1; 0 0 0; 1 2 1]
    parallelForRows(height, gradientCostPerRow, context.threadCount, [&](int rowBegin, int rowEnd) {
        for (int y = rowBegin; y < rowEnd; ++y) {
            const std::uint8_t* up   = lumaRow(std::max(y - 1, 0));
            const std::uint8_t* mid  = lumaRow(y);
            const std::uint8_t* down = lumaRow(std::min(y + 1, height - 1));
            std::uint8_t* dst = image.rowPtr(y);

            for (int x = 0; x < width; ++x) {
                const int left  = std::max(x - 1, 0);
                const int right = std::min(x + 1, width - 1);

                const int gx = (up[right] + 2 * mid[right] + down[right]) -
                               (up[left]  + 2 * mid[left]  + down[left]);
                const int gy = (down[left] + 2 * down[x] + down[right]) -
                               (up[left]   + 2 * up[x]   + up[right]);

                const std::uint8_t magnitude =
                    pixel::saturate(std::sqrt(static_cast<float>(gx * gx + gy * gy)));
                dst[x * ImageBuffer::CHANNELS + pixel::BLUE]  = magnitude;
                dst[x * ImageBuffer::CHANNELS + pixel::GREEN] = magnitude;
                dst[x * ImageBuffer::CHANNELS + pixel::RED]   = magnitude;
            }
        }
    });
}

} // namespace ip
