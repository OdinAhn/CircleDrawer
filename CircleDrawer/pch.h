#pragma once

// 미리 컴파일된 헤더 — 자주 바뀌지 않는 MFC/ATL 헤더를 모아 둔다.

#include "targetver.h"

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN  // 거의 사용되지 않는 내용을 Windows 헤더에서 제외
#endif

#include <afxwin.h>       // MFC 핵심 및 표준 구성 요소
#include <afxext.h>       // MFC 확장
#include <afxdialogex.h>  // CDialogEx
#include <atlimage.h>     // CImage

// 버튼/에디트 컨트롤에 Windows 비주얼 스타일 적용
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
