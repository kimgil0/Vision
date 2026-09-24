/**
 * @file SceneTests.cpp
 * @brief 화면 상태 규칙: 최대 3점, 정원 조건, 드래그 대상 선택, 그리기 결과.
 */

#include "TestFramework.h"

#include "CircleScene.h"

#include <stdexcept>
#include <thread>
#include <vector>

using circle::CircleScene;
using circle::GrayCanvas;

TEST_CASE(Scene_AcceptsAtMostThreePoints) {
    CircleScene scene;
    CHECK(scene.addPoint({ 10, 10 }));
    CHECK(scene.addPoint({ 50, 10 }));
    CHECK(!scene.circle());                 // 두 점만으로는 원 없음
    CHECK(scene.addPoint({ 30, 40 }));
    CHECK(scene.circle().has_value());      // 세 번째 클릭 이후 정원
    CHECK(!scene.addPoint({ 70, 70 }));     // 네 번째 클릭부터 무시
    CHECK_EQ(scene.points().size(), std::size_t{ 3 });
}

TEST_CASE(Scene_NoCircleForCollinearPoints) {
    CircleScene scene;
    scene.addPoint({ 0, 0 });
    scene.addPoint({ 10, 10 });
    scene.addPoint({ 20, 20 });
    CHECK(scene.isComplete());
    CHECK(!scene.circle());
}

TEST_CASE(Scene_HitTestPicksNearestPointWithinGrabRadius) {
    CircleScene scene;
    scene.addPoint({ 100, 100 });
    scene.addPoint({ 112, 100 });
    scene.addPoint({ 300, 300 });
    CHECK_EQ(*scene.hitTest({ 109, 100 }, 10), std::size_t{ 1 });  // 둘 다 범위 안 → 가까운 쪽
    CHECK_EQ(*scene.hitTest({ 298, 303 }, 10), std::size_t{ 2 });
    CHECK(!scene.hitTest({ 200, 200 }, 10));
}

TEST_CASE(Scene_MovePointValidatesIndexAndResetClears) {
    CircleScene scene;
    scene.addPoint({ 1, 1 });
    scene.movePoint(0, { 5, 6 });
    CHECK_EQ(scene.points()[0].x, 5.0);
    CHECK_THROWS_AS(scene.movePoint(1, { 0, 0 }), std::out_of_range);
    scene.reset();
    CHECK(scene.points().empty());
    CHECK(scene.addPoint({ 2, 2 }));  // 초기화 후 다시 입력 가능
}

TEST_CASE(Scene_RenderDrawsPointsAndRingButNotInterior) {
    const int w = 200, h = 200;
    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(w) * h);
    GrayCanvas canvas(buffer.data(), w, h, w);

    CircleScene scene;
    scene.addPoint({ 100, 40 });
    scene.addPoint({ 160, 100 });
    scene.addPoint({ 100, 160 });   // 중심 (100, 100), 반지름 60
    circle::DrawSettings settings;
    settings.pointRadius = 6;
    settings.circleThickness = 3;
    scene.render(canvas, settings);

    CHECK_EQ(canvas.pixel(100, 40), 0);    // 클릭 지점 원
    CHECK_EQ(canvas.pixel(40, 100), 0);    // 정원 둘레 (점이 없는 쪽)
    CHECK_EQ(canvas.pixel(100, 100), 255); // 정원 내부는 비어 있음
    CHECK(canvas.pixel(0, 0) < 255);       // 캔버스 테두리

    scene.reset();
    scene.render(canvas, settings);
    CHECK_EQ(canvas.pixel(40, 100), 255);  // 초기화 후 모두 지워짐
}

TEST_CASE(Scene_WorkerFrameMatchesUiFramePixelForPixel) {
    // [랜덤 이동] 때 작업 스레드가 전용 버퍼에 그린 프레임은, 같은 좌표·설정으로 UI 스레드가
    // 그린 프레임과 픽셀 단위로 같아야 한다. 두 스레드가 동시에 그려도 서로 영향이 없어야 한다.
    const int w = 123, h = 77, pitch = 124;   // CImage 처럼 4바이트 정렬된 pitch
    std::vector<std::uint8_t> uiBuffer(static_cast<std::size_t>(pitch) * h, 0x11);
    std::vector<std::uint8_t> workerBuffer(static_cast<std::size_t>(pitch) * h, 0x22);

    CircleScene scene;
    scene.setPoints({ { { 20, 20 }, { 100, 30 }, { 60, 70 } } });
    circle::DrawSettings settings;
    settings.pointRadius = 5;
    settings.circleThickness = 4;

    std::thread worker([&] {
        GrayCanvas canvas(workerBuffer.data(), w, h, pitch);
        scene.render(canvas, settings);
    });
    GrayCanvas uiCanvas(uiBuffer.data(), w, h, pitch);
    scene.render(uiCanvas, settings);
    worker.join();

    int mismatches = 0;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            mismatches += uiBuffer[static_cast<std::size_t>(y) * pitch + x] !=
                          workerBuffer[static_cast<std::size_t>(y) * pitch + x];
    CHECK_EQ(mismatches, 0);
}