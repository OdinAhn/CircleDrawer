// CircleDrawer.cpp : 응용 프로그램 진입점

#include "pch.h"
#include "CircleDrawer.h"
#include "CircleDrawerDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// 유일한 CCircleDrawerApp 개체
CCircleDrawerApp theApp;

BOOL CCircleDrawerApp::InitInstance()
{
	CWinApp::InitInstance();

	// 대화 상자 기반 프로그램: 메인 대화 상자를 모달로 띄우고, 닫히면 종료한다.
	CCircleDrawerDlg dlg;
	m_pMainWnd = &dlg;
	dlg.DoModal();

	// 메시지 펌프를 시작하지 않고 프로그램을 끝내도록 FALSE 를 반환한다.
	return FALSE;
}
