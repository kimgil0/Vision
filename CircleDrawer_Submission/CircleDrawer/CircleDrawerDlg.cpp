/**
 * @file CircleDrawerDlg.cpp
 */

#include "pch.h"
#include "CircleDrawerDlg.h"
#include "RandomPointGenerator.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <memory>
#include <random>

using circle::DrawSettings;
using circle::Point2D;

namespace {

constexpr int RANDOM_STEPS = 10;                                     // 총 10번
constexpr std::chrono::milliseconds RANDOM_INTERVAL{ 500 };          // 초당 2회
constexpr double MIN_GRAB_RADIUS = 8.0;                              // 반지름이 작아도 잡기 쉽게

CString formatPoint(const Point2D& p) {
    CString text;
    text.Format(_T("(%ld, %ld)"), std::lround(p.x), std::lround(p.y));
    return text;
}

} // anonymous namespace

BEGIN_MESSAGE_MAP(CCircleDrawerDlg, CDialogEx)
    ON_WM_PAINT()
    ON_WM_DESTROY()
    ON_WM_LBUTTONDOWN()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
    ON_WM_CAPTURECHANGED()
    ON_WM_SETCURSOR()
    ON_BN_CLICKED(IDC_BTN_RESET, &CCircleDrawerDlg::OnBnClickedReset)
    ON_BN_CLICKED(IDC_BTN_RANDOM, &CCircleDrawerDlg::OnBnClickedRandom)
    ON_EN_CHANGE(IDC_EDIT_RADIUS, &CCircleDrawerDlg::OnEnChangeRadius)
    ON_EN_CHANGE(IDC_EDIT_THICKNESS, &CCircleDrawerDlg::OnEnChangeThickness)
    ON_MESSAGE(WM_APP_RANDOM_STEP, &CCircleDrawerDlg::OnRandomStep)
    ON_MESSAGE(WM_APP_RANDOM_DONE, &CCircleDrawerDlg::OnRandomDone)
END_MESSAGE_MAP()

CCircleDrawerDlg::CCircleDrawerDlg(CWnd* pParent)
    : CDialogEx(IDD_CIRCLEDRAWER_DIALOG, pParent)
{
}

void CCircleDrawerDlg::DoDataExchange(CDataExchange* pDX) {
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_SPIN_RADIUS, m_spinRadius);
    DDX_Control(pDX, IDC_SPIN_THICKNESS, m_spinThickness);
}

// ── 초기화 ──────────────────────────────────────────────────

BOOL CCircleDrawerDlg::OnInitDialog() {
    CDialogEx::OnInitDialog();

    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
    SetIcon(m_hIcon, TRUE);   // 큰 아이콘 (Alt+Tab, 작업 표시줄)
    SetIcon(m_hIcon, FALSE);  // 작은 아이콘 (제목 표시줄)

    // 입력 범위는 스핀 버튼으로 1차 제한하고, 직접 입력한 값은 EN_CHANGE 에서 검증한다.
    m_spinRadius.SetRange32(DrawSettings::MIN_POINT_RADIUS, DrawSettings::MAX_POINT_RADIUS);
    m_spinThickness.SetRange32(DrawSettings::MIN_THICKNESS, DrawSettings::MAX_THICKNESS);
    SetDlgItemInt(IDC_EDIT_RADIUS, static_cast<UINT>(m_settings.pointRadius), FALSE);
    SetDlgItemInt(IDC_EDIT_THICKNESS, static_cast<UINT>(m_settings.circleThickness), FALSE);

    if (!createCanvas()) {
        AfxMessageBox(_T("캔버스 이미지를 만들 수 없습니다."), MB_ICONERROR);
        EndDialog(IDCANCEL);
        return TRUE;
    }

    m_initialized = true;
    publishSettings();
    redraw();
    updateLabels();
    updateControls();
    setStatus(_T("캔버스를 클릭해\r\n점 3개를 찍으세요."));
    return TRUE;
}

bool CCircleDrawerDlg::createCanvas() {
    // 대화상자 템플릿의 숨은 자리표시 컨트롤 크기를 픽셀로 받아 캔버스 크기로 쓴다.
    // (대화상자 단위 기반이라 DPI·글꼴이 달라도 배치가 어긋나지 않는다)
    CWnd* placeholder = GetDlgItem(IDC_CANVAS);
    if (placeholder == nullptr) {
        return false;
    }
    placeholder->GetWindowRect(&m_canvasRect);
    ScreenToClient(&m_canvasRect);

    // 안내 영상과 같은 방식: 8비트 그레이스케일 CImage. 높이를 음수로 주면 top-down (0행이 맨 위).
    if (!m_image.Create(m_canvasRect.Width(), -m_canvasRect.Height(), 8)) {
        return false;
    }
    RGBQUAD palette[256] = {};
    for (int i = 0; i < 256; ++i) {
        palette[i].rgbRed = palette[i].rgbGreen = palette[i].rgbBlue = static_cast<BYTE>(i);
    }
    m_image.SetColorTable(0, 256, palette);
    return true;
}

// ── 그리기 ──────────────────────────────────────────────────

void CCircleDrawerDlg::redraw() {
    if (m_image.IsNull()) {
        return;
    }
    // 픽셀 메모리에 직접 그린다 (Ellipse·GDI+ 등 도형 API 미사용).
    circle::GrayCanvas canvas(static_cast<std::uint8_t*>(m_image.GetBits()),
                              m_image.GetWidth(), m_image.GetHeight(), m_image.GetPitch());
    m_scene.render(canvas, m_settings);

    // 배경을 지우지 않고(FALSE) 캔버스 영역만 무효화 → 깜빡임 없음.
    // UpdateWindow 로 즉시 그려서 드래그 중에도 매 이동마다 화면에 반영한다.
    InvalidateRect(m_canvasRect, FALSE);
    UpdateWindow();
}

void CCircleDrawerDlg::OnPaint() {
    CPaintDC dc(this);
    if (!m_image.IsNull()) {
        m_image.Draw(dc.GetSafeHdc(), m_canvasRect.left, m_canvasRect.top);
    }
}

void CCircleDrawerDlg::updateLabels() {
    const auto& points = m_scene.points();
    const int labelIds[] = { IDC_POINT1, IDC_POINT2, IDC_POINT3 };
    for (int i = 0; i < 3; ++i) {
        CString text;
        if (static_cast<std::size_t>(i) < points.size()) {
            text.Format(_T("P%d  %s"), i + 1, static_cast<LPCTSTR>(formatPoint(points[static_cast<std::size_t>(i)])));
        }
        else {
            text.Format(_T("P%d  -"), i + 1);
        }
        SetDlgItemText(labelIds[i], text);
    }

    CString info;
    if (const auto c = m_scene.circle()) {
        info.Format(_T("중심 %s\r\n반지름 %.1f"), static_cast<LPCTSTR>(formatPoint(c->center)), c->radius);
    }
    else if (m_scene.isComplete()) {
        info = _T("세 점이 한 직선 위에 있어\r\n원을 그릴 수 없습니다.");
    }
    else {
        info.Format(_T("점 %zu / 3"), points.size());
    }
    SetDlgItemText(IDC_CIRCLE_INFO, info);
}

void CCircleDrawerDlg::updateControls() {
    const bool running = m_mover.isRunning();
    GetDlgItem(IDC_BTN_RANDOM)->EnableWindow(!running && m_scene.circle().has_value());
}

void CCircleDrawerDlg::setStatus(const CString& text) {
    SetDlgItemText(IDC_STATUS, text);
}

// ── 마우스: 클릭으로 점 추가, 드래그로 이동 ──────────────────

circle::Point2D CCircleDrawerDlg::toCanvas(CPoint clientPoint) const {
    // 드래그 중 캔버스 밖으로 나가도 점은 캔버스 가장자리에 머문다.
    const long x = std::clamp(clientPoint.x - m_canvasRect.left, 0L, static_cast<long>(m_canvasRect.Width() - 1));
    const long y = std::clamp(clientPoint.y - m_canvasRect.top, 0L, static_cast<long>(m_canvasRect.Height() - 1));
    return { static_cast<double>(x), static_cast<double>(y) };
}

double CCircleDrawerDlg::grabRadius() const {
    // (std::max): windows.h 의 max 매크로가 std::max 를 가로채지 않도록 괄호로 감싼다.
    return (std::max)(static_cast<double>(m_settings.pointRadius), MIN_GRAB_RADIUS);
}

void CCircleDrawerDlg::OnLButtonDown(UINT nFlags, CPoint point) {
    CDialogEx::OnLButtonDown(nFlags, point);
    if (!m_canvasRect.PtInRect(point)) {
        return;
    }
    if (m_mover.isRunning()) {
        setStatus(_T("랜덤 이동 중에는 점을\r\n추가하거나 움직일 수 없습니다."));
        return;
    }

    const Point2D p = toCanvas(point);
    if (const auto hit = m_scene.hitTest(p, grabRadius())) {
        m_dragIndex = *hit;   // 기존 점을 누름 → 드래그 시작
        SetCapture();         // 커서가 캔버스·창 밖으로 나가도 이동 메시지를 계속 받는다
        return;
    }

    if (m_scene.addPoint(p)) {  // 네 번째 클릭부터는 false → 클릭 지점 원을 그리지 않음
        redraw();
        updateLabels();
        updateControls();
        if (m_scene.isComplete()) {
            setStatus(m_scene.circle() ? _T("점을 드래그하면\r\n정원이 따라 움직입니다.")
                                       : _T("세 점이 한 직선 위에 있습니다.\r\n점을 드래그해 보세요."));
        }
    }
}

void CCircleDrawerDlg::OnMouseMove(UINT nFlags, CPoint point) {
    CDialogEx::OnMouseMove(nFlags, point);
    if (!m_dragIndex || (nFlags & MK_LBUTTON) == 0) {
        return;
    }
    m_scene.movePoint(*m_dragIndex, toCanvas(point));
    redraw();       // 드래그가 끝날 때까지 매 이동마다 정원을 다시 그린다
    updateLabels();
}

void CCircleDrawerDlg::OnLButtonUp(UINT nFlags, CPoint point) {
    CDialogEx::OnLButtonUp(nFlags, point);
    if (m_dragIndex) {
        m_dragIndex.reset();
        if (GetCapture() == this) {
            ReleaseCapture();
        }
        updateControls();
    }
}

void CCircleDrawerDlg::OnCaptureChanged(CWnd* pWnd) {
    // Alt+Tab 등으로 마우스 캡처를 잃으면 드래그를 끝낸다.
    if (pWnd != this) {
        m_dragIndex.reset();
    }
    CDialogEx::OnCaptureChanged(pWnd);
}

BOOL CCircleDrawerDlg::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message) {
    // 점 위에서는 손 모양 커서로 "끌 수 있음"을 알려 준다.
    if (pWnd == this && nHitTest == HTCLIENT && !m_mover.isRunning()) {
        CPoint cursor;
        ::GetCursorPos(&cursor);
        ScreenToClient(&cursor);
        if (m_dragIndex || (m_canvasRect.PtInRect(cursor) && m_scene.hitTest(toCanvas(cursor), grabRadius()))) {
            ::SetCursor(::LoadCursor(nullptr, IDC_HAND));
            return TRUE;
        }
    }
    return CDialogEx::OnSetCursor(pWnd, nHitTest, message);
}

// ── 입력값 ──────────────────────────────────────────────────

bool CCircleDrawerDlg::readSetting(int editId, int minValue, int maxValue, int& target, LPCTSTR name) {
    BOOL translated = FALSE;
    const UINT value = GetDlgItemInt(editId, &translated, FALSE);
    if (!translated || value < static_cast<UINT>(minValue) || value > static_cast<UINT>(maxValue)) {
        CString message;
        message.Format(_T("%s:\r\n%d~%d 사이의 정수를 입력하세요."), name, minValue, maxValue);
        setStatus(message);
        return false;  // 잘못된 값은 적용하지 않고 이전 값을 유지
    }
    target = static_cast<int>(value);
    return true;
}

void CCircleDrawerDlg::OnEnChangeRadius() {
    if (m_initialized && readSetting(IDC_EDIT_RADIUS, DrawSettings::MIN_POINT_RADIUS,
                                     DrawSettings::MAX_POINT_RADIUS, m_settings.pointRadius, _T("클릭 지점 반지름"))) {
        publishSettings();  // 랜덤 이동 중이면 작업 스레드의 다음 프레임부터 반영
        redraw();
    }
}

void CCircleDrawerDlg::OnEnChangeThickness() {
    if (m_initialized && readSetting(IDC_EDIT_THICKNESS, DrawSettings::MIN_THICKNESS,
                                     DrawSettings::MAX_THICKNESS, m_settings.circleThickness, _T("정원 두께"))) {
        publishSettings();
        redraw();
    }
}

void CCircleDrawerDlg::OnOK() {
    // 편집 중 Enter 키(IDOK)로 대화상자가 닫히지 않도록 아무것도 하지 않는다.
}

// ── 버튼 ────────────────────────────────────────────────────

void CCircleDrawerDlg::OnBnClickedReset() {
    stopRandomMove();
    m_dragIndex.reset();
    m_scene.reset();
    redraw();
    updateLabels();
    updateControls();
    setStatus(_T("초기화했습니다.\r\n점 3개를 다시 찍으세요."));
}

void CCircleDrawerDlg::stopRandomMove() {
    m_mover.stop();   // 대기 중인 작업 스레드를 즉시 깨워 join
    ++m_session;      // 이미 큐에 들어간 이전 세션 메시지는 무시된다
    std::lock_guard<std::mutex> lock(m_pendingMutex);
    m_pending.reset();
}

void CCircleDrawerDlg::OnBnClickedRandom() {
    if (m_mover.isRunning() || !m_scene.circle()) {
        return;
    }
    m_dragIndex.reset();
    const unsigned session = ++m_session;
    const HWND hwnd = GetSafeHwnd();

    // 작업 스레드만 사용하는 생성기 (UI 스레드와 공유하는 상태 없음).
    // 여백 = 반지름 + 2 → 클릭 지점 원이 캔버스 밖으로 잘리지 않는다.
    auto generator = std::make_shared<circle::RandomPointGenerator>(
        m_image.GetWidth(), m_image.GetHeight(), m_settings.pointRadius + 2, std::random_device{}());
    const int width  = m_image.GetWidth();
    const int height = m_image.GetHeight();
    const int pitch  = m_image.GetPitch();   // top-down 이므로 양수

    const bool started = m_mover.start(RANDOM_STEPS, RANDOM_INTERVAL,
        // ↓ 작업 스레드에서 실행: "랜덤 이동 + 정원 그리기"를 모두 여기서 한다.
        //   그리는 대상은 이 스레드 전용 버퍼이며, 화면용 CImage·MFC 객체는 절대 만지지 않는다.
        //   UI 스레드는 완성된 프레임을 받아 화면에 옮기기만 한다 (PostMessage → OnRandomStep).
        [this, hwnd, generator, session, width, height, pitch](int step, int total) {
            PendingFrame frame;
            frame.points  = generator->next();
            frame.step    = step;
            frame.total   = total;
            frame.session = session;
            frame.pixels.resize(static_cast<std::size_t>(pitch) * static_cast<std::size_t>(height));

            circle::CircleScene scene;
            scene.setPoints(frame.points);
            circle::GrayCanvas canvas(frame.pixels.data(), width, height, pitch);
            scene.render(canvas, sharedSettings());   // 이동 중에 바뀐 반지름·두께도 반영

            {
                std::lock_guard<std::mutex> lock(m_pendingMutex);
                m_pending = std::move(frame);
            }
            ::PostMessage(hwnd, WM_APP_RANDOM_STEP, session, 0);  // Send 가 아닌 Post → 교착 불가
        },
        [hwnd, session](bool completed) {
            ::PostMessage(hwnd, WM_APP_RANDOM_DONE, session, completed ? 1 : 0);
        });

    if (started) {
        updateControls();
        setStatus(_T("랜덤 이동 시작\r\n(초당 2회, 총 10번)"));
    }
}

LRESULT CCircleDrawerDlg::OnRandomStep(WPARAM wParam, LPARAM /*lParam*/) {
    if (static_cast<unsigned>(wParam) != m_session) {
        return 0;  // 초기화 등으로 무효가 된 이전 세션의 메시지
    }
    std::optional<PendingFrame> frame;
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        frame.swap(m_pending);
    }
    if (!frame || frame->session != m_session) {
        return 0;
    }

    // 화면에 보이는 그림과 UI 상태(좌표 표시, 다음 드래그)를 같은 좌표로 맞춘다.
    m_scene.setPoints(frame->points);
    presentFrame(frame->pixels);
    updateLabels();
    CString status;
    status.Format(_T("랜덤 이동 %d / %d"), frame->step, frame->total);
    setStatus(status);
    return 0;
}

void CCircleDrawerDlg::presentFrame(const std::vector<std::uint8_t>& pixels) {
    const std::size_t bytes = static_cast<std::size_t>(m_image.GetPitch()) * static_cast<std::size_t>(m_image.GetHeight());
    if (pixels.size() != bytes) {
        redraw();  // 크기가 다르면(이론상 없음) UI 스레드가 직접 다시 그린다
        return;
    }
    std::memcpy(m_image.GetBits(), pixels.data(), bytes);   // 완성된 프레임을 한 번에 복사
    InvalidateRect(m_canvasRect, FALSE);
    UpdateWindow();
}

void CCircleDrawerDlg::publishSettings() {
    std::lock_guard<std::mutex> lock(m_settingsMutex);
    m_sharedSettings = m_settings;
}

circle::DrawSettings CCircleDrawerDlg::sharedSettings() {
    std::lock_guard<std::mutex> lock(m_settingsMutex);
    return m_sharedSettings;
}

LRESULT CCircleDrawerDlg::OnRandomDone(WPARAM wParam, LPARAM lParam) {
    if (static_cast<unsigned>(wParam) != m_session) {
        return 0;
    }
    m_mover.stop();  // 이미 끝난 스레드를 join
    updateControls();
    if (lParam != 0) {
        setStatus(_T("랜덤 이동 완료 (10번)"));
    }
    return 0;
}

void CCircleDrawerDlg::OnDestroy() {
    // 창이 사라지기 전에 작업 스레드를 반드시 멈추고 join 한다.
    stopRandomMove();
    CDialogEx::OnDestroy();
}
