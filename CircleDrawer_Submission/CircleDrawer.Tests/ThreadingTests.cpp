/**
 * @file ThreadingTests.cpp
 * @brief 랜덤 점 생성과 작업 스레드: 횟수·간격, 즉시 중단, 중복 실행 방지, 예외 격리.
 */

#include "TestFramework.h"

#include "RandomMover.h"
#include "RandomPointGenerator.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;

TEST_CASE(Generator_PointsStayInsideMarginsApartAndFormACircle) {
    circle::RandomPointGenerator generator(640, 480, 12, 2026);
    for (int i = 0; i < 500; ++i) {
        const auto p = generator.next();
        for (const auto& q : p) {
            CHECK(q.x >= 12 && q.x <= 640 - 1 - 12);
            CHECK(q.y >= 12 && q.y <= 480 - 1 - 12);
        }
        CHECK(circle::distance(p[0], p[1]) >= 28 && circle::distance(p[1], p[2]) >= 28 &&
              circle::distance(p[2], p[0]) >= 28);
        CHECK(circle::circumcircle(p[0], p[1], p[2]).has_value());
    }
}

TEST_CASE(Generator_IsReproducibleWithSameSeed) {
    circle::RandomPointGenerator a(300, 200, 5, 42), b(300, 200, 5, 42);
    for (int i = 0; i < 20; ++i) {
        const auto pa = a.next(), pb = b.next();
        for (int k = 0; k < 3; ++k) {
            CHECK(pa[k].x == pb[k].x && pa[k].y == pb[k].y);
        }
    }
}

TEST_CASE(Mover_RunsAllStepsOnWorkerThreadAtFixedInterval) {
    circle::RandomMover mover;
    std::mutex mutex;
    std::vector<int> steps;
    std::vector<Clock::time_point> times;
    std::atomic<bool> onOtherThread{ true };
    std::atomic<int> doneCount{ 0 };
    std::atomic<bool> completedFlag{ false };
    const auto mainThread = std::this_thread::get_id();

    const auto start = Clock::now();
    CHECK(mover.start(5, 40ms,
        [&](int step, int total) {
            std::lock_guard<std::mutex> lock(mutex);
            steps.push_back(step);
            times.push_back(Clock::now());
            if (std::this_thread::get_id() == mainThread || total != 5) onOtherThread = false;
        },
        [&](bool completed) { completedFlag = completed; ++doneCount; }));

    while (mover.isRunning()) std::this_thread::sleep_for(5ms);
    mover.stop();

    CHECK_EQ(steps.size(), std::size_t{ 5 });
    for (int i = 0; i < 5; ++i) CHECK_EQ(steps[static_cast<std::size_t>(i)], i + 1);
    CHECK(onOtherThread.load());
    CHECK(completedFlag.load());
    CHECK_EQ(doneCount.load(), 1);
    // 첫 단계는 즉시, 이후 40ms 간격 → 마지막 단계는 약 160ms 시점 (절대 시각 기준이라 누적 지연 없음)
    CHECK(times.back() - start >= 155ms);
    CHECK(times.back() - start < 400ms);
}

TEST_CASE(Mover_StopWakesWorkerImmediatelyAndPreventsFurtherSteps) {
    circle::RandomMover mover;
    std::atomic<int> calls{ 0 };
    std::atomic<bool> completedFlag{ true };
    CHECK(mover.start(10, 500ms, [&](int, int) { ++calls; }, [&](bool completed) { completedFlag = completed; }));
    while (calls.load() == 0) std::this_thread::sleep_for(1ms);

    const auto before = Clock::now();
    mover.stop();   // 500ms 대기 중이어도 곧바로 깨어나 join 되어야 한다
    const auto stopTime = Clock::now() - before;

    CHECK(stopTime < 100ms);
    CHECK_EQ(calls.load(), 1);
    CHECK(!completedFlag.load());
    CHECK(!mover.isRunning());
    std::this_thread::sleep_for(600ms);
    CHECK_EQ(calls.load(), 1);  // stop 이후 추가 호출 없음
}

TEST_CASE(Mover_RejectsSecondStartWhileRunningButRestartsAfterStop) {
    circle::RandomMover mover;
    std::atomic<int> calls{ 0 };
    CHECK(mover.start(3, 200ms, [&](int, int) { ++calls; }, nullptr));
    while (calls.load() == 0) std::this_thread::sleep_for(1ms);          // 첫 단계 실행 확인
    CHECK(!mover.start(3, 200ms, [&](int, int) { ++calls; }, nullptr));   // 실행 중 중복 시작 거부
    mover.stop();                                                          // 200ms 대기 중 → 즉시 종료
    CHECK_EQ(calls.load(), 1);
    CHECK(mover.start(1, 1ms, [&](int, int) { ++calls; }, nullptr));      // 정지 후 다시 시작 가능
    while (mover.isRunning()) std::this_thread::sleep_for(1ms);
    mover.stop();
    CHECK_EQ(calls.load(), 2);
}

TEST_CASE(Mover_DestructorJoinsRunningThread) {
    std::atomic<int> calls{ 0 };
    const auto before = Clock::now();
    {
        circle::RandomMover mover;
        mover.start(10, 1000ms, [&](int, int) { ++calls; }, nullptr);
        while (calls.load() == 0) std::this_thread::sleep_for(1ms);
    }   // 소멸자가 stop + join (joinable std::thread 소멸 → std::terminate 방지)
    CHECK(Clock::now() - before < 300ms);
}

TEST_CASE(Mover_CallbackExceptionStopsWorkerWithoutTerminating) {
    circle::RandomMover mover;
    std::atomic<bool> completedFlag{ true };
    std::atomic<bool> done{ false };
    mover.start(5, 1ms, [](int step, int) { if (step == 2) throw std::runtime_error("boom"); },
                [&](bool completed) { completedFlag = completed; done = true; });
    while (!done.load()) std::this_thread::sleep_for(1ms);
    mover.stop();
    CHECK(!completedFlag.load());
}
