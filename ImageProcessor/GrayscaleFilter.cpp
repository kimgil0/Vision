/**
 * @file GrayscaleFilter.cpp
 */

#include "GrayscaleFilter.h"
#include "Parallel.h"
#include "PixelMath.h"

namespace ip {

void GrayscaleFilter::process(ImageBuffer& image, const FilterContext& context) const {
    const int width = image.width();
    const std::size_t costPerRow = static_cast<std::size_t>(image.rowStride());

    parallelForRows(image.height(), costPerRow, context.threadCount, [&](int rowBegin, int rowEnd) {
        for (int y = rowBegin; y < rowEnd; ++y) {
            std::uint8_t* px = image.rowPtr(y);
            for (int x = 0; x < width; ++x, px += ImageBuffer::CHANNELS) {
                const std::uint8_t gray =
                    pixel::luma(px[pixel::BLUE], px[pixel::GREEN], px[pixel::RED]);
                px[pixel::BLUE]  = gray;
                px[pixel::GREEN] = gray;
                px[pixel::RED]   = gray;
            }
        }
    });
}

} // namespace ip
