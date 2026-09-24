#pragma once

/**
 * @file GrayCanvas.h
 * @brief 8비트 그레이스케일 픽셀 버퍼에 원을 직접 그린다 (GDI/GDI+ 도형 API 미사용).
 *
 * 과제 조건: 클릭 지점 원과 정원은 Ellipse 류·GDI+·Polygon API 없이,
 * 안내 영상(MFC Study step 2-1 ~ 2-3)처럼 CImage 의 픽셀 메모리(GetBits/GetPitch)에 직접 값을 써서 그린다.
 * 이 클래스는 그 메모리를 가리키는 비소유 뷰이며, MFC 에 의존하지 않아 단위 테스트가 가능하다.
 */

#include "Geometry.h"

#include <cstdint>

namespace circle {

class GrayCanvas {
public:
    static constexpr std::uint8_t WHITE = 255;
    static constexpr std::uint8_t BLACK = 0;

    /**
     * @param bits   첫 행의 시작 주소 (top-down)
     * @param pitch  한 행의 바이트 수 (4바이트 정렬 패딩 포함, width 이상)
     * @throws std::invalid_argument 인자가 유효하지 않은 경우.
     */
    GrayCanvas(std::uint8_t* bits, int width, int height, int pitch);

    int width() const noexcept { return m_width; }
    int height() const noexcept { return m_height; }

    /// @throws std::out_of_range 좌표가 캔버스 밖인 경우.
    std::uint8_t pixel(int x, int y) const;

    void fill(std::uint8_t value) noexcept;

    /// 캔버스 가장자리 1픽셀 테두리.
    void drawBorder(std::uint8_t ink) noexcept;

    /**
     * @brief 속이 채워진 원 (클릭 지점 원).
     * 가장자리 픽셀은 원 안에 들어간 비율만큼 ink 를 섞어 계단 현상을 줄인다 (안티에일리어싱).
     */
    void fillDisk(const Point2D& center, double radius, std::uint8_t ink) noexcept;

    /**
     * @brief 속이 빈 원 (정원). [radius − thickness/2, radius + thickness/2] 고리만 칠한다.
     *
     * 캔버스 전체가 아니라 행마다 고리가 지나는 구간(span)만 계산하므로 비용이 O(둘레 × 두께) 이다.
     * 세 점이 거의 일직선이라 반지름이 매우 커도 캔버스 안의 행·열만 순회하며, 캔버스를 벗어난 부분은 잘린다.
     */
    void strokeCircle(const Point2D& center, double radius, double thickness, std::uint8_t ink) noexcept;

private:
    /// 현재 값과 ink 를 coverage(0~1) 비율로 섞는다. 이미 더 진한 픽셀은 밝히지 않는다.
    void blend(int x, int y, double coverage, std::uint8_t ink) noexcept;

    std::uint8_t* m_bits;
    int           m_width;
    int           m_height;
    int           m_pitch;
};

} // namespace circle
