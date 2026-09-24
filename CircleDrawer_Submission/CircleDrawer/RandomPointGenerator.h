#pragma once

/**
 * @file RandomPointGenerator.h
 * @brief [랜덤 이동]용 무작위 세 점 생성. 워커 스레드가 단독으로 소유한다 (공유 상태 없음).
 */

#include "Geometry.h"

#include <array>
#include <cstdint>
#include <random>

namespace circle {

class RandomPointGenerator {
public:
    /**
     * @param width, height  캔버스 크기 (px)
     * @param margin         가장자리 여백. 클릭 지점 원이 캔버스 밖으로 잘리지 않도록 반지름을 넘긴다.
     * @param seed           재현 가능한 테스트를 위해 외부에서 주입한다.
     */
    RandomPointGenerator(int width, int height, int margin, std::uint32_t seed);

    /**
     * @brief 캔버스 안에 있고, 서로 충분히 떨어져 있으며, 한 직선에 가깝지 않은 세 점.
     * 너무 납작한 삼각형은 정원이 거대해져 화면에 거의 직선으로 보이므로 다시 뽑는다.
     */
    std::array<Point2D, 3> next();

private:
    Point2D randomPoint();

    std::mt19937 m_engine;
    std::uniform_int_distribution<int> m_xDist;
    std::uniform_int_distribution<int> m_yDist;
    double m_minSeparation;
};

} // namespace circle
