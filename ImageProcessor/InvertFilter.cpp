/**
 * @file InvertFilter.cpp
 */

#include "InvertFilter.h"
#include "Parallel.h"
#include "PixelMath.h"

namespace ip {

void InvertFilter::process(ImageBuffer& image, const FilterContext& context) const {
    const int rowStride = image.rowStride();
    const std::size_t costPerRow = static_cast<std::size_t>(rowStride);

    parallelForRows(image.height(), costPerRow, context.threadCount, [&](int rowBegin, int rowEnd) {
        for (int y = rowBegin; y < rowEnd; ++y) {
            std::uint8_t* row = image.rowPtr(y);
            for (int i = 0; i < rowStride; ++i) {
                row[i] = static_cast<std::uint8_t>(pixel::MAX_VALUE - row[i]);
            }
        }
    });
}

} // namespace ip
