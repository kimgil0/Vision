#pragma once

/**
 * @file PixelMath.h
 * @brief 필터들이 공유하는 픽셀 단위 헬퍼 (채널 인덱스, 휘도, 포화 연산).
 *
 * ImageBuffer 는 BGR 순서로 저장된다. 채널 인덱스를 숫자 리터럴(0, 1, 2)로 쓰지 않고
 * 이 파일의 상수를 사용하여 RGB/BGR 혼동을 원천 차단한다.
 */

#include <cstdint>

namespace ip {
namespace pixel {

/// BGR 채널 오프셋.
constexpr int BLUE  = 0;
constexpr int GREEN = 1;
constexpr int RED   = 2;

constexpr std::uint8_t MIN_VALUE = 0;
constexpr std::uint8_t MAX_VALUE = 255;

/**
 * @brief ITU-R BT.601 휘도(luma)를 정수 고정소수점(Q8)으로 계산한다.
 *
 *   Y = 0.299 R + 0.587 G + 0.114 B   →   (77 R + 150 G + 29 B + 128) >> 8
 *
 * 가중치 합이 정확히 256 이므로 흰색은 255, 검정은 0 으로 정확히 보존되고
 * (+128 은 반올림), 부동소수점과 달리 플랫폼과 무관하게 결과가 비트 단위로 동일하다.
 */
constexpr std::uint8_t luma(std::uint8_t blue, std::uint8_t green, std::uint8_t red) noexcept {
    return static_cast<std::uint8_t>((29 * blue + 150 * green + 77 * red + 128) >> 8);
}

/// 정수를 [0, 255] 로 포화(saturate)시킨다. uint8 오버플로(wrap-around)를 방지한다.
constexpr std::uint8_t saturate(int value) noexcept {
    return static_cast<std::uint8_t>(value < 0 ? 0 : (value > 255 ? 255 : value));
}

/// 실수를 반올림한 뒤 [0, 255] 로 포화시킨다.
inline std::uint8_t saturate(float value) noexcept {
    if (value <= 0.0f)   { return MIN_VALUE; }
    if (value >= 255.0f) { return MAX_VALUE; }
    return static_cast<std::uint8_t>(value + 0.5f);  // 양수 구간이므로 +0.5 절삭 = 반올림
}

} // namespace pixel
} // namespace ip
