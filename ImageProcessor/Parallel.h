#pragma once

/**
 * @file Parallel.h
 * @brief 행(row) 단위 데이터 병렬 처리 유틸리티.
 */

#include <cstddef>
#include <functional>

namespace ip {

/// 스레드 수 상한 (CLI 검증 및 비정상 값 방어용).
constexpr unsigned MAX_THREAD_COUNT = 256;

/**
 * @brief 스레드 하나에 배정할 최소 작업량 (대략적인 픽셀·채널 연산 수, 1 연산 ≈ 0.6 ns).
 *
 * 실측(8코어): 스레드 하나를 추가하는 비용이 약 0.5 ms 였다. 기준을 256K 로 두었을 때
 * 512×512 grayscale(0.5 ms)이 8 스레드에서 1.7 ms 로 오히려 3배 느려졌다.
 * 2M(≈1.3 ms 분량)으로 올리자 작은 이미지는 단일 스레드와 같은 속도를 유지하고,
 * 4000×3000 이미지는 모든 필터가 2.5~3배 빨라졌다.
 */
constexpr std::size_t MIN_COST_PER_TASK = 2 * 1024 * 1024;

/**
 * @brief 요청된 스레드 수를 실제 사용할 값으로 정규화한다.
 * @param requested 0 이면 하드웨어 동시 실행 수를 사용한다 (알 수 없으면 1).
 * @return [1, MAX_THREAD_COUNT] 범위의 값.
 */
unsigned resolveThreadCount(unsigned requested) noexcept;

/**
 * @brief [0, rowCount) 를 겹치지 않는 연속 행 블록으로 나누어 병렬 실행한다.
 *
 * 보장 사항:
 *  - body(rowBegin, rowEnd) 는 서로 겹치지 않는 구간으로 호출된다.
 *    body 가 자기 구간의 출력 행에만 쓰면 데이터 경쟁이 없고,
 *    결과는 스레드 수와 무관하게 비트 단위로 동일하다.
 *  - 작업 스레드에서 던진 예외는 모든 스레드를 join 한 뒤 호출 스레드로 재전파된다.
 *    (스레드 함수 밖으로 빠져나간 예외는 std::terminate 를 유발한다.)
 *  - 스레드 생성 자체가 실패해도(std::system_error) 이미 시작된 스레드는 join 된다.
 *  - 전체 작업량(rowCount × costPerRow)이 작으면 호출 스레드에서 바로 실행한다.
 *
 * @param costPerRow 한 행을 처리하는 대략적인 연산 수 (예: rowStride × 커널 탭 수).
 */
void parallelForRows(int rowCount, std::size_t costPerRow, unsigned threadCount,
                     const std::function<void(int rowBegin, int rowEnd)>& body);

} // namespace ip
