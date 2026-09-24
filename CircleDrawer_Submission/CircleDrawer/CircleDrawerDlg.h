#pragma once

/**
 * @file CircleDrawerDlg.h
 * @brief 메인 대화상자. 입력 처리와 화면 표시만 담당하고, 계산·그리기는 CircleScene 에 위임한다.
 *
 * 스레드 구분
 *  - UI 스레드: 클릭·드래그·입력 변경 시 그리기, 완성된 프레임을 화면에 표시
 *  - 작업 스레드(RandomMover): [랜덤 이동]의 좌표 생성 + 정원 그리기를 전용 버퍼에서 수행
 *    → MFC 객체는 만지지 않고 ::PostMessage 로만 UI 스레드에 알린다
 *      (Microsoft Learn, "Multithreading: MFC Programming Tips" 권장 방식)
 */

#include "CircleScene.h"
#include "RandomMover.h"
#include "resource.h"

#include <array>
#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

/// 작업 스레드 → UI 스레드: "새 프레임이 완성됨". wParam: 세션 번호
constexpr UINT WM_APP_RANDOM_STEP = WM_APP + 1;
/// 작업 스레드 종료 알림. wParam: 세션 번호, lParam: 10번 모두 완료했으면 1
constexpr UINT WM_APP_RANDOM_DONE = WM_APP + 2;

class CCircleDrawerDlg : public CDialogEx {
public:
    explicit CCircleDrawerDlg(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_CIRCLEDRAWER_DIALOG };
#endif

protected:
    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    void OnOK() override;  // Enter 키로 창이 닫히지 않도록 무시

    afx_msg void OnPaint();
    afx_msg void OnDestroy();
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnCaptureChanged(CWnd* pWnd);
    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
    afx_msg void OnBnClickedReset();
    afx_msg void OnBnClickedRandom();
    afx_msg void OnEnChangeRadius();
    afx_msg void OnEnChangeThickness();
    afx_msg LRESULT OnRandomStep(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnRandomDone(WPARAM wParam, LPARAM lParam);
    DECLARE_MESSAGE_MAP()

private:
    /// 작업 스레드가 완성한 한 단계: 새 좌표 + 그 좌표로 그린 프레임(픽셀).
    /// m_pendingMutex 로 보호하며 UI 스레드가 꺼내 화면에 옮긴다.
    struct PendingFrame {
        std::array<circle::Point2D, 3> points;
        std::vector<std::uint8_t>      pixels;  ///< 캔버스와 같은 크기·pitch 의 8비트 이미지
        int      step    = 0;
        int      total   = 0;
        unsigned session = 0;
    };

    bool createCanvas();
    void redraw();
    void presentFrame(const std::vector<std::uint8_t>& pixels);
    void publishSettings();
    circle::DrawSettings sharedSettings();
    void updateLabels();
    void updateControls();
    void setStatus(const CString& text);
    void stopRandomMove();
    bool readSetting(int editId, int minValue, int maxValue, int& target, LPCTSTR name);
    circle::Point2D toCanvas(CPoint clientPoint) const;
    double grabRadius() const;

    HICON                m_hIcon = nullptr;
    CImage               m_image;        ///< 8비트 그레이스케일 캔버스 (픽셀 메모리에 직접 그림)
    CRect                m_canvasRect;   ///< 대화상자 클라이언트 좌표
    CSpinButtonCtrl      m_spinRadius;
    CSpinButtonCtrl      m_spinThickness;
    circle::CircleScene  m_scene;
    circle::DrawSettings m_settings;
    std::optional<std::size_t> m_dragIndex;
    bool                 m_initialized = false;

    circle::RandomMover         m_mover;
    std::mutex                  m_pendingMutex;
    std::optional<PendingFrame> m_pending;
    std::mutex                  m_settingsMutex;
    circle::DrawSettings        m_sharedSettings;   ///< 작업 스레드가 읽는 설정 사본 (m_settingsMutex 로 보호)
    unsigned                    m_session = 0;      ///< 초기화·재시작마다 증가 → 늦게 도착한 이전 메시지 무시
};
