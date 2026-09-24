#pragma once

/**
 * @file GrayscaleFilter.h
 */

#include "FilterBase.h"

namespace ip {

/// BT.601 휘도로 그레이스케일 변환한다. 출력은 B = G = R 인 24비트 이미지.
class GrayscaleFilter final : public FilterBase {
public:
    std::string describe() const override { return "grayscale"; }

private:
    void process(ImageBuffer& image, const FilterContext& context) const override;
};

} // namespace ip
