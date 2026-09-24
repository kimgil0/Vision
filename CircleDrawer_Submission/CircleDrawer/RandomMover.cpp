/**
 * @file RandomMover.cpp
 */

#include "RandomMover.h"

namespace circle {

RandomMover::~RandomMover() {
    stop();
}

bool RandomMover::start(int steps, std::chrono::milliseconds interval, StepCallback onStep, DoneCallback onDone) {
    if (steps <= 0 || !onStep || m_running.load()) {
        return false;
    }
    if (m_thread.joinable()) {
        m_thread.join();  // 이전 실행이 끝났지만 아직 join 되지 않은 스레드 정리
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopRequested = false;
    }
    m_running = true;
    m_thread = std::thread(&RandomMover::run, this, steps, interval, std::move(onStep), std::move(onDone));
    return true;
}

void RandomMover::stop() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopRequested = true;
    }
    m_wakeUp.notify_all();
    if (m_thread.joinable() && m_thread.get_id() != std::this_thread::get_id()) {
        m_thread.join();
    }
    m_running = false;
}

void RandomMover::run(int steps, std::chrono::milliseconds interval, StepCallback onStep, DoneCallback onDone) {
    const auto startTime = std::chrono::steady_clock::now();
    bool completed = true;

    for (int step = 1; step <= steps; ++step) {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            const auto due = startTime + interval * (step - 1);
            if (m_wakeUp.wait_until(lock, due, [this] { return m_stopRequested; })) {
                completed = false;  // stop() 요청으로 깨어남
                break;
            }
        }
        try {
            onStep(step, steps);
        }
        catch (...) {
            // 스레드 함수 밖으로 예외가 나가면 std::terminate. 여기서 멈추고 완료 콜백으로 알린다.
            completed = false;
            break;
        }
    }

    m_running = false;
    if (onDone) {
        try {
            onDone(completed);
        }
        catch (...) {
        }
    }
}

} // namespace circle
