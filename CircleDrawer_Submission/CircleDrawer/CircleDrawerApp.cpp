/**
 * @file CircleDrawerApp.cpp
 */

#include "pch.h"
#include "CircleDrawerApp.h"
#include "CircleDrawerDlg.h"

BEGIN_MESSAGE_MAP(CCircleDrawerApp, CWinApp)
END_MESSAGE_MAP()

CCircleDrawerApp theApp;

BOOL CCircleDrawerApp::InitInstance() {
    // 공용 컨트롤 v6 (스핀 버튼 등) 초기화
    INITCOMMONCONTROLSEX init{};
    init.dwSize = sizeof(init);
    init.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&init);

    CWinApp::InitInstance();

    CCircleDrawerDlg dialog;
    m_pMainWnd = &dialog;
    dialog.DoModal();

    // 대화상자가 닫히면 메시지 루프를 시작하지 않고 종료한다.
    return FALSE;
}

int CCircleDrawerApp::ExitInstance() {
    // 기본 구현은 마지막으로 처리한 메시지의 wParam 을 종료 코드로 돌려주므로 값이 일정하지 않다.
    // 정상 종료는 항상 0 을 반환한다.
    CWinApp::ExitInstance();
    return 0;
}
