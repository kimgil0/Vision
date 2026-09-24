#pragma once

/**
 * @file InvertFilter.h
 */

#include "FilterBase.h"

namespace ip {

/// 모든 채널을 반전한다 (v → 255 − v). 두 번 적용하면 원본으로 돌아온다.
class InvertFilter final : public FilterBase {
public:
    std::string describe() const override { return "invert"; }

private:
    void process(ImageBuffer& image, const FilterContext& context) const override;
};

} // namespace ip
