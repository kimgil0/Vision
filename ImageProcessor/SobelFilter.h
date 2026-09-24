#pragma once

/**
 * @file SobelFilter.h
 */

#include "FilterBase.h"

namespace ip {

/**
 * @brief Sobel 엣지 검출.
 *
 * 휘도 평면의 그래디언트 크기 √(Gx² + Gy²) 를 [0, 255] 로 포화시켜 그레이스케일로 출력한다.
 * 경계는 가장자리 복제로 처리하므로 이미지 테두리에 가짜 엣지가 생기지 않는다.
 */
class SobelFilter final : public FilterBase {
public:
    std::string describe() const override { return "sobel"; }

private:
    void process(ImageBuffer& image, const FilterContext& context) const override;
};

} // namespace ip
