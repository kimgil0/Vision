#pragma once

/**
 * @file GaussianBlurFilter.h
 */

#include "FilterBase.h"

#include <vector>

namespace ip {

/**
 * @brief 분리형(separable) 가우시안 블러.
 *
 * 2D 가우시안 = 1D(가로) ∘ 1D(세로) 이므로 픽셀당 연산량이 O(r²) → O(r) 로 줄어든다.
 * 커널 반경 r = ceil(3σ) (가우시안 질량의 99.7% 포함), 경계는 가장자리 복제(clamp-to-edge).
 * 커널 가중치 합이 1 이므로 균일한 영역의 밝기는 보존되고 테두리가 어두워지지 않는다.
 */
class GaussianBlurFilter final : public FilterBase {
public:
    static constexpr double DEFAULT_SIGMA = 1.0;
    static constexpr double MAX_SIGMA     = 50.0;

    /// @throws FilterError sigma 가 (0, MAX_SIGMA] 범위를 벗어난 경우 (NaN 포함).
    explicit GaussianBlurFilter(double sigma = DEFAULT_SIGMA);

    std::string describe() const override;

private:
    void process(ImageBuffer& image, const FilterContext& context) const override;

    double             m_sigma;
    std::vector<float> m_kernel;  ///< 길이 2r+1, 합이 1 로 정규화됨. 생성 시 1회 계산.
};

} // namespace ip
