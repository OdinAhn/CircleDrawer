#pragma once

// CircleDrawer 응용 프로그램 클래스

#ifndef __AFXWIN_H__
#error "PCH에 대해 이 파일을 포함하기 전에 'pch.h'를 포함합니다."
#endif

#include "resource.h"

class CCircleDrawerApp : public CWinApp
{
public:
	BOOL InitInstance() override;
};

extern CCircleDrawerApp theApp;
