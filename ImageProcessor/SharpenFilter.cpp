/**
 * @file SharpenFilter.cpp
 */

#include "SharpenFilter.h"
#include "Exceptions.h"
#include "Parallel.h"
#include "PixelMath.h"

namespace ip {

namespace {
    constexpr double MASK_SIGMA = 1.0;
} // anonymous namespace

SharpenFilter::SharpenFilter(double amount)
    : m_amount(amount)
    , m_blur(MASK_SIGMA)
{
    if (!(amount > 0.0 && amount <= MAX_AMOUNT)) {
        throw FilterError("sharpen: amount must be in (0, " + formatNumber(MAX_AMOUNT) +
                          "] (got " + formatNumber(amount) + ")");
    }
}

std::string SharpenFilter::describe() const {
    return "sharpen(amount=" + formatNumber(m_amount) + ")";
}

void SharpenFilter::process(ImageBuffer& image, const FilterContext& context) const {
    ImageBuffer blurred = image;  // 저주파 성분. 원본은 그대로 두고 복사본을 블러한다.
    m_blur.apply(blurred, context);

    const float amount    = static_cast<float>(m_amount);
    const int   rowStride = image.rowStride();
    const std::size_t costPerRow = static_cast<std::size_t>(rowStride) * 2;

    parallelForRows(image.height(), costPerRow, context.threadCount, [&](int rowBegin, int rowEnd) {
        for (int y = rowBegin; y < rowEnd; ++y) {
            std::uint8_t* row = image.rowPtr(y);
            const std::uint8_t* lowPass = blurred.rowPtr(y);
            for (int i = 0; i < rowStride; ++i) {
                const float original = static_cast<float>(row[i]);
                const float detail   = original - static_cast<float>(lowPass[i]);
                row[i] = pixel::saturate(original + amount * detail);
            }
        }
    });
}

} // namespace ip
