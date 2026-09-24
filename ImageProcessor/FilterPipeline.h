#pragma once

/**
 * @file FilterPipeline.h
 */

#include "FilterBase.h"

#include <cstddef>
#include <memory>
#include <vector>

namespace ip {

/**
 * @brief 여러 필터를 순서대로 적용하는 복합 필터 (Composite 패턴).
 *
 * FilterPipeline 자체가 FilterBase 이므로, main 은 단일 필터와 파이프라인을
 * 구분하지 않고 동일한 인터페이스로 다룬다.
 */
class FilterPipeline final : public FilterBase {
public:
    FilterPipeline() = default;

    /// @throws FilterError filter 가 nullptr 인 경우.
    void add(std::unique_ptr<FilterBase> filter);

    std::size_t size() const noexcept { return m_stages.size(); }
    bool empty() const noexcept { return m_stages.empty(); }

    /// 예: "grayscale -> blur(sigma=1.5) -> threshold(otsu)"
    std::string describe() const override;

private:
    /// 각 단계를 순서대로 적용하고, logger 가 있으면 단계별 소요 시간을 기록한다.
    /// @throws FilterError 파이프라인이 비어 있는 경우.
    void process(ImageBuffer& image, const FilterContext& context) const override;

    std::vector<std::unique_ptr<FilterBase>> m_stages;
};

} // namespace ip
