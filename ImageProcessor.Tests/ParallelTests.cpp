/**
 * @file ParallelTests.cpp
 * @brief 행 분할의 완전성, 워커 예외 전파, 스레드 수 정규화.
 */

#include "TestFramework.h"

#include "Parallel.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

TEST_CASE(Parallel_CoversEveryRowExactlyOnce) {
    // costPerRow 를 최소 작업량으로 주어 한 행까지 쪼갤 수 있게 하고, 나누어떨어지지 않는 조합을 검사한다.
    for (const int rows : { 1, 31, 32, 33, 1000, 1237 }) {
        for (const unsigned threads : { 1u, 2u, 3u, 8u, 64u }) {
            std::vector<int> visits(static_cast<std::size_t>(rows), 0);
            ip::parallelForRows(rows, ip::MIN_COST_PER_TASK, threads, [&](int rowBegin, int rowEnd) {
                for (int y = rowBegin; y < rowEnd; ++y) {
                    ++visits[static_cast<std::size_t>(y)];
                }
            });
            CHECK_MSG(std::all_of(visits.begin(), visits.end(), [](int v) { return v == 1; }),
                      "rows=" + std::to_string(rows) + " threads=" + std::to_string(threads));
        }
    }
}

TEST_CASE(Parallel_PropagatesWorkerExceptionInsteadOfTerminating) {
    // 워커 스레드의 예외가 전파되지 않으면 std::terminate 로 테스트 프로세스 자체가 죽는다.
    const auto failOnLaterBlocks = [](int rowBegin, int) {
        if (rowBegin > 0) {
            throw std::runtime_error("worker failed");
        }
    };
    CHECK_THROWS_CONTAINS(ip::parallelForRows(1000, ip::MIN_COST_PER_TASK, 4, failOnLaterBlocks),
                          std::runtime_error, "worker failed");
}

TEST_CASE(Parallel_SmallWorkloadRunsOnCallingThreadOnly) {
    // 전체 작업량이 MIN_COST_PER_TASK 미만이면 스레드를 만들지 않고 한 블록으로 실행한다.
    int calls = 0;
    ip::parallelForRows(100, 10, 8, [&](int rowBegin, int rowEnd) {
        ++calls;
        CHECK(rowBegin == 0 && rowEnd == 100);
    });
    CHECK_EQ(calls, 1);
}

TEST_CASE(Parallel_ResolveThreadCountClampsToValidRange) {
    CHECK(ip::resolveThreadCount(0) >= 1u);
    CHECK_EQ(ip::resolveThreadCount(1), 1u);
    CHECK_EQ(ip::resolveThreadCount(100000), ip::MAX_THREAD_COUNT);
}
