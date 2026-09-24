/**
 * @file Parallel.cpp
 */

#include "Parallel.h"

#include <algorithm>
#include <exception>
#include <limits>
#include <thread>
#include <vector>

namespace ip {

namespace {

/// 스코프를 벗어날 때(예외 포함) 모든 스레드를 join 하는 RAII 가드.
/// joinable 한 std::thread 가 소멸되면 std::terminate 가 호출되므로 반드시 필요하다.
class ThreadJoinGuard {
public:
    explicit ThreadJoinGuard(std::vector<std::thread>& threads) noexcept
        : m_threads(threads) {}

    ~ThreadJoinGuard() {
        for (std::thread& thread : m_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }

    ThreadJoinGuard(const ThreadJoinGuard&) = delete;
    ThreadJoinGuard& operator=(const ThreadJoinGuard&) = delete;

private:
    std::vector<std::thread>& m_threads;
};

} // anonymous namespace

unsigned resolveThreadCount(unsigned requested) noexcept {
    unsigned count = requested;
    if (count == 0) {
        count = std::thread::hardware_concurrency();  // 알 수 없으면 0 을 반환할 수 있다.
    }
    return std::clamp(count, 1u, MAX_THREAD_COUNT);
}

void parallelForRows(int rowCount, std::size_t costPerRow, unsigned threadCount,
                     const std::function<void(int, int)>& body) {
    if (rowCount <= 0) {
        return;
    }

    // 전체 작업량이 MIN_COST_PER_TASK 의 몇 배인지로 유용한 블록 수를 정한다 (곱셈 오버플로 시 포화).
    const std::size_t rows = static_cast<std::size_t>(rowCount);
    const std::size_t totalCost = (costPerRow != 0 && rows > std::numeric_limits<std::size_t>::max() / costPerRow)
                                      ? std::numeric_limits<std::size_t>::max()
                                      : rows * costPerRow;
    const std::size_t usefulTasks = std::min(totalCost / MIN_COST_PER_TASK, rows);
    const unsigned taskCount = static_cast<unsigned>(std::max<std::size_t>(
        1, std::min<std::size_t>({ threadCount, usefulTasks, MAX_THREAD_COUNT })));

    if (taskCount == 1) {
        body(0, rowCount);
        return;
    }

    std::vector<std::exception_ptr> errors(taskCount);
    const auto runTask = [&](unsigned task) {
        // 64비트로 계산하여 곱셈 오버플로를 막고, 나머지 행을 블록들에 고르게 분배한다.
        const int rowBegin = static_cast<int>(static_cast<long long>(rowCount) * task / taskCount);
        const int rowEnd   = static_cast<int>(static_cast<long long>(rowCount) * (task + 1) / taskCount);
        try {
            body(rowBegin, rowEnd);
        }
        catch (...) {
            errors[task] = std::current_exception();
        }
    };

    std::vector<std::thread> workers;
    workers.reserve(taskCount - 1);
    {
        ThreadJoinGuard joinGuard(workers);
        for (unsigned task = 1; task < taskCount; ++task) {
            workers.emplace_back(runTask, task);
        }
        runTask(0);  // 호출 스레드도 놀지 않고 첫 번째 블록을 처리한다.
    }

    // 여러 블록이 실패했다면 가장 앞 블록의 예외를 대표로 전파한다.
    for (const std::exception_ptr& error : errors) {
        if (error) {
            std::rethrow_exception(error);
        }
    }
}

} // namespace ip
