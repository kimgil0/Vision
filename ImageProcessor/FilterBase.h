#pragma once

/**
 * @file FilterBase.h
 * @brief 모든 이미지 필터의 추상 기반 클래스.
 */

#include "ImageBuffer.h"

#include <string>

namespace ip {

class Logger;

/**
 * @brief 필터 실행 환경.
 *
 * 필터 고유 파라미터(임계값, sigma 등)는 생성자로, 실행 환경(스레드 수, 로거)은
 * 여기로 분리한다. 같은 필터 객체를 다른 환경에서 그대로 재사용할 수 있다.
 */
struct FilterContext {
    unsigned threadCount = 1;        ///< 1 이상. resolveThreadCount() 로 정규화한 값을 넣는다.
    Logger*  logger      = nullptr;  ///< 선택. nullptr 이면 필터 내부 진단 로그를 생략한다.
};

/**
 * @brief 이미지 필터 추상 기반 클래스 (NVI: Non-Virtual Interface 패턴).
 *
 *  - 공개 apply() 가 모든 필터 공통의 사전조건을 한 곳에서 검사한 뒤
 *    private 가상함수 process() 를 호출한다. 파생 클래스는 process() 만 구현한다.
 *  - 필터는 생성 시점에 파라미터 검증이 끝난 불변(immutable) 객체이고 apply() 는 const 이므로,
 *    한 인스턴스를 여러 이미지/스레드에서 공유해도 안전하다.
 *  - std::unique_ptr<FilterBase> 로 다형적으로 소유되므로 복사를 금지해 슬라이싱을 막는다.
 */
class FilterBase {
public:
    virtual ~FilterBase() = default;

    FilterBase(const FilterBase&) = delete;
    FilterBase& operator=(const FilterBase&) = delete;

    /**
     * @brief image 에 필터를 제자리(in-place) 적용한다.
     * @throws FilterError 빈 이미지이거나 context 가 유효하지 않은 경우.
     */
    void apply(ImageBuffer& image, const FilterContext& context = {}) const;

    /// 로그/오류 메시지용 설명. 파라미터를 포함한다 (예: "blur(sigma=1.5)").
    virtual std::string describe() const = 0;

protected:
    FilterBase() = default;

    /// describe() 구현용 숫자 포맷: 1.5 → "1.5", 2.0 → "2".
    static std::string formatNumber(double value);

private:
    /// 사전조건이 보장된 상태로 호출된다: image 는 비어 있지 않고 threadCount >= 1.
    virtual void process(ImageBuffer& image, const FilterContext& context) const = 0;
};

} // namespace ip
