#pragma once

/**
 * @file RandomMover.h
 * @brief [랜덤 이동]을 일정 간격으로 반복 실행하는 작업 스레드.
 *
 * 설계 원칙
 *  - 워커 스레드는 UI 객체(MFC CWnd, CImage)를 절대 직접 만지지 않는다. 콜백에서 PostMessage 로
 *    UI 스레드에 알리기만 한다 → MFC 객체의 스레드 비안전성 문제와 교착(deadlock)을 원천 차단.
 *  - 대기는 Sleep 이 아니라 condition_variable::wait_until 로 한다 → stop() 즉시 깨어나 종료.
 *  - 각 단계는 시작 시각 기준 절대 시각(start + k·interval)에 실행 → 콜백 시간이 누적되어 늦어지지 않는다.
 *  - 소멸자·stop() 이 반드시 join 한다 → 창이 닫힌 뒤 스레드가 해제된 객체를 건드리는 일이 없다.
 */

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace circle {

class RandomMover {
public:
    /// 워커 스레드에서 호출된다. step 은 1부터 total 까지.
    using StepCallback = std::function<void(int step, int total)>;
    /// 워커 스레드에서 마지막에 한 번 호출된다. completed: 모든 단계를 마쳤으면 true, 중단되면 false.
    using DoneCallback = std::function<void(bool completed)>;

    RandomMover() = default;
    ~RandomMover();

    RandomMover(const RandomMover&) = delete;
    RandomMover& operator=(const RandomMover&) = delete;

    /**
     * @brief steps 번, interval 간격으로 onStep 을 호출하는 스레드를 시작한다. 첫 단계는 즉시 실행.
     * @return 이미 실행 중이면 false (중복 실행 방지).
     */
    bool start(int steps, std::chrono::milliseconds interval, StepCallback onStep, DoneCallback onDone);

    /// 대기 중인 스레드를 즉시 깨워 종료시키고 join 한다. 여러 번, 실행 중이 아닐 때 호출해도 안전.
    void stop();

    bool isRunning() const noexcept { return m_running.load(); }

private:
    void run(int steps, std::chrono::milliseconds interval, StepCallback onStep, DoneCallback onDone);

    std::thread             m_thread;
    std::mutex              m_mutex;
    std::condition_variable m_wakeUp;
    bool                    m_stopRequested = false;  ///< m_mutex 로 보호
    std::atomic<bool>       m_running{ false };
};

} // namespace circle
