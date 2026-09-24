#pragma once

/**
 * @file FilterFactory.h
 * @brief 문자열 스펙으로부터 필터/파이프라인을 생성한다.
 *
 * 스펙 문법:
 *   filter   := name [ ':' param ]          예) grayscale, threshold:128, blur:1.5
 *   pipeline := filter { ',' filter }       예) "grayscale, blur:1.5, threshold:otsu"
 *
 *  - 이름은 대소문자를 구분하지 않고, 각 토큰 앞뒤 공백은 무시한다.
 *  - 파라미터는 문자열 전체가 숫자여야 한다 ("128abc" 거부).
 *  - 새 필터 추가 = FilterFactory.cpp 의 카탈로그에 한 줄 추가 (main/CLI 수정 불필요).
 */

#include "FilterBase.h"
#include "FilterPipeline.h"

#include <iosfwd>
#include <memory>
#include <string>

namespace ip {

class FilterFactory {
public:
    FilterFactory() = delete;  // 인스턴스화 금지 (정적 메서드만 제공)

    /// @throws FilterError 알 수 없는 이름, 잘못된 파라미터, 여러 필터가 지정된 경우.
    static std::unique_ptr<FilterBase> create(const std::string& spec);

    /// @throws FilterError 빈 항목, 알 수 없는 이름, 잘못된 파라미터.
    static std::unique_ptr<FilterPipeline> createPipeline(const std::string& specList);

    /// 지원 필터 목록과 사용법을 출력한다 (--list-filters).
    static void printCatalog(std::ostream& os);
};

} // namespace ip
