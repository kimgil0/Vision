#pragma once

/**
 * @file ThresholdFilter.h
 */

#include "FilterBase.h"

#include <optional>

namespace ip {

/**
 * @brief 휘도 기준 이진화. luma > level 이면 흰색(255), 아니면 검정(0).
 *        (OpenCV THRESH_BINARY 와 동일한 비교 규약)
 *
 * level 을 지정하지 않으면 Otsu 방법으로 히스토그램에서 임계값을 자동 결정한다.
 * 문서 스캔(4_text_page.bmp)처럼 밝기 분포가 이미지마다 다른 입력에서 고정값보다 견고하다.
 */
class ThresholdFilter final : public FilterBase {
public:
    static constexpr int MIN_LEVEL = 0;
    static constexpr int MAX_LEVEL = 255;

    /**
     * @param level [0, 255] 의 고정 임계값. std::nullopt 이면 Otsu 자동 임계값.
     * @throws FilterError level 이 범위를 벗어난 경우.
     */
    explicit ThresholdFilter(std::optional<int> level);

    std::string describe() const override;

    /// Otsu 방법으로 클래스 간 분산을 최대화하는 임계값을 구한다. (테스트를 위해 공개)
    static int computeOtsuLevel(const ImageBuffer& image);

private:
    void process(ImageBuffer& image, const FilterContext& context) const override;

    std::optional<int> m_level;
};

} // namespace ip
