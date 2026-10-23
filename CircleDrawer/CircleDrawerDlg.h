#pragma once

// CircleDrawerDlg.h : 메인 대화 상자
//
// 동작 요약
//   1~3번째 클릭 : 클릭 지점에 채워진 원(클릭 지점 원)을 그린다.
//   3번째 클릭 후 : 세 점을 모두 지나는 정원(외접원)을 테두리만 그린다.
//   4번째 클릭부터: 클릭 지점 원을 그리지 않는다. 대신 기존 점을 잡고 드래그할 수 있다.
//   [초기화]      : 모든 내용을 지우고 처음 상태로 돌아간다.
//   [랜덤 이동]   : 별도 쓰레드에서 0.5초마다 세 점을 무작위로 옮기고 정원을 다시 그린다 (10회).
//
// 그리기는 Ellipse 등 GDI 도형 함수를 쓰지 않고,
// CImage 의 픽셀 메모리(GetBits)에 밝기 값을 직접 기록하는 방식으로 한다.

#include <atomic>
#include <mutex>
#include <thread>

class CCircleDrawerDlg : public CDialogEx
{
public:
	explicit CCircleDrawerDlg(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_CIRCLEDRAWER_DIALOG };
#endif

protected:
	BOOL OnInitDialog() override;
	void OnOK() override;  // 에디트에서 Enter 를 눌러도 창이 닫히지 않도록 막는다.

	afx_msg void OnPaint();
	afx_msg void OnDestroy();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnBnClickedBtnReset();
	afx_msg void OnBnClickedBtnRandom();
	afx_msg LRESULT OnRedrawImage(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnRandomMoveDone(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()

private:
	static constexpr int MAX_POINT = 3;                  // 클릭 지점 원 개수
	static constexpr int GRAY_BACKGROUND = 0xff;         // 배경 밝기 (흰색)
	static constexpr int GRAY_POINT = 80;                // 클릭 지점 원 밝기
	static constexpr int GRAY_CIRCLE = 0;                // 정원 테두리 밝기 (검은색)
	static constexpr int MIN_RADIUS = 1;
	static constexpr int MAX_RADIUS = 100;
	static constexpr int MIN_THICKNESS = 1;
	static constexpr int MAX_THICKNESS = 50;
	static constexpr int RANDOM_MOVE_COUNT = 10;         // 랜덤 이동 반복 횟수
	static constexpr int RANDOM_MOVE_INTERVAL_MS = 500;  // 초당 2회

	// ── 그리기 (m_mtxImage 를 잡은 상태에서 호출) ──────────────
	void initImage();
	void renderImage();
	void drawCircle(unsigned char* fm, int nCenterX, int nCenterY, int nRadius, int nGray);
	void drawRing(unsigned char* fm, double dCenterX, double dCenterY, double dRadius,
	              int nThickness, int nGray);
	bool isInCircle(int i, int j, int nCenterX, int nCenterY, int nRadius) const;
	bool isInRing(int i, int j, double dCenterX, double dCenterY,
	              double dInner, double dOuter) const;
	BOOL validImgPos(int x, int y) const;
	static bool calcCircumcircle(const CPoint pt[MAX_POINT],
	                             double& dCenterX, double& dCenterY, double& dRadius);

	// ── UI 보조 ───────────────────────────────────────────────
	bool readInputs(int& nRadius, int& nThickness);  // 잘못된 값이면 메시지 박스 후 false
	void updateLabels();
	void refreshView();
	int hitTestPoint(CPoint ptImg) const;
	CPoint toImagePos(CPoint ptClient) const;

	// ── 랜덤 이동 쓰레드 ──────────────────────────────────────
	void randomMoveThread();
	void stopRandomMove();

	CImage m_image;
	CRect m_rcView;                  // 대화 상자 클라이언트 좌표에서 이미지가 그려지는 영역

	// 아래 그리기 상태는 UI 쓰레드와 랜덤 이동 쓰레드가 함께 쓰므로 m_mtxImage 로 보호한다.
	std::mutex m_mtxImage;
	CPoint m_ptClick[MAX_POINT];
	int m_nClickCount = 0;
	int m_nRadius = 10;
	int m_nThickness = 3;
	bool m_bCircleValid = false;     // 정원을 그릴 수 있는 상태인지 (세 점이 일직선이면 false)
	double m_dCircleX = 0.0;
	double m_dCircleY = 0.0;
	double m_dCircleR = 0.0;

	int m_nDragIndex = -1;           // 드래그 중인 점 번호 (-1: 드래그 아님). UI 쓰레드 전용

	std::thread m_threadRandom;
	std::atomic<bool> m_bStopRandom{ false };
};
