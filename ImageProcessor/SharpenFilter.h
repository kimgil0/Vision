#pragma once

/**
 * @file SharpenFilter.h
 */

#include "FilterBase.h"
#include "GaussianBlurFilter.h"

namespace ip {

/**
 * @brief 언샤프 마스크(unsharp mask) 샤프닝.
 *
 *   out = src + amount · (src − GaussianBlur(src, σ = 1))
 *
 * 블러는 GaussianBlurFilter 를 합성(composition)하여 재사용한다.
 */
class SharpenFilter final : public FilterBase {
public:
    static constexpr double DEFAULT_AMOUNT = 1.0;
    static constexpr double MAX_AMOUNT     = 10.0;

    /// @throws FilterError amount 가 (0, MAX_AMOUNT] 범위를 벗어난 경우 (NaN 포함).
    explicit SharpenFilter(double amount = DEFAULT_AMOUNT);

    std::string describe() const override;

private:
    void process(ImageBuffer& image, const FilterContext& context) const override;

    double             m_amount;
    GaussianBlurFilter m_blur;
};

} // namespace ip
