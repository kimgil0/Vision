#pragma once

/**
 * @file CircleDrawerApp.h
 * @brief 애플리케이션 진입점. 대화상자 하나를 모달로 띄운다.
 */

#ifndef __AFXWIN_H__
#error "PCH에 대해 이 파일을 포함하기 전에 'pch.h'를 포함합니다."
#endif

class CCircleDrawerApp : public CWinApp {
public:
    BOOL InitInstance() override;
    int ExitInstance() override;

    DECLARE_MESSAGE_MAP()
};

extern CCircleDrawerApp theApp;
