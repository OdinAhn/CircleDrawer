// CircleDrawerDlg.cpp : 메인 대화 상자 구현

#include "pch.h"
#include "CircleDrawer.h"
#include "CircleDrawerDlg.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace {
	// 작업 쓰레드 → UI 쓰레드 알림 메시지
	constexpr UINT WM_USER_REDRAW = WM_APP + 1;       // 이미지가 갱신되었으니 다시 그려라
	constexpr UINT WM_USER_RANDOM_DONE = WM_APP + 2;  // 랜덤 이동이 끝났다
}

CCircleDrawerDlg::CCircleDrawerDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_CIRCLEDRAWER_DIALOG, pParent)
{
}

BEGIN_MESSAGE_MAP(CCircleDrawerDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_DESTROY()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_BN_CLICKED(IDC_BTN_RESET, &CCircleDrawerDlg::OnBnClickedBtnReset)
	ON_BN_CLICKED(IDC_BTN_RANDOM, &CCircleDrawerDlg::OnBnClickedBtnRandom)
	ON_MESSAGE(WM_USER_REDRAW, &CCircleDrawerDlg::OnRedrawImage)
	ON_MESSAGE(WM_USER_RANDOM_DONE, &CCircleDrawerDlg::OnRandomMoveDone)
END_MESSAGE_MAP()


// ─────────────────────────────────────────────────────────────
// 초기화 / 종료
// ─────────────────────────────────────────────────────────────

BOOL CCircleDrawerDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	SetDlgItemInt(IDC_EDIT_RADIUS, m_nRadius);
	SetDlgItemInt(IDC_EDIT_THICKNESS, m_nThickness);

	// 리소스에 배치한 프레임 컨트롤 위치를 그리기 영역으로 사용하고, 컨트롤 자체는 숨긴다.
	CWnd* pView = GetDlgItem(IDC_STATIC_VIEW);
	pView->GetWindowRect(&m_rcView);
	ScreenToClient(&m_rcView);
	pView->ShowWindow(SW_HIDE);

	{
		std::lock_guard<std::mutex> lock(m_mtxImage);
		initImage();
		renderImage();
	}
	updateLabels();

	return TRUE;
}

void CCircleDrawerDlg::OnOK()
{
	// 의도적으로 비워 둔다. (CDialogEx::OnOK 를 호출하면 창이 닫힌다)
}

void CCircleDrawerDlg::OnDestroy()
{
	// 창이 사라지기 전에 작업 쓰레드를 반드시 종료시킨다.
	stopRandomMove();
	CDialogEx::OnDestroy();
}


// ─────────────────────────────────────────────────────────────
// 화면 출력
// ─────────────────────────────────────────────────────────────

void CCircleDrawerDlg::OnPaint()
{
	CPaintDC dc(this);

	std::lock_guard<std::mutex> lock(m_mtxImage);
	if (m_image) {
		m_image.Draw(dc, m_rcView.left, m_rcView.top);
	}
}

void CCircleDrawerDlg::refreshView()
{
	// 배경을 지우지 않고(FALSE) 이미지 영역만 다시 그려 깜빡임을 줄인다.
	InvalidateRect(&m_rcView, FALSE);
}

void CCircleDrawerDlg::updateLabels()
{
	CString strPoint[MAX_POINT];
	CString strStatus;
	{
		std::lock_guard<std::mutex> lock(m_mtxImage);
		for (int k = 0; k < MAX_POINT; k++) {
			if (k < m_nClickCount)
				strPoint[k].Format(_T("P%d : (%d, %d)"), k + 1, m_ptClick[k].x, m_ptClick[k].y);
			else
				strPoint[k].Format(_T("P%d : -"), k + 1);
		}

		if (m_nClickCount < MAX_POINT)
			strStatus.Format(_T("그리기 영역을 클릭하세요. (%d / %d)"), m_nClickCount, MAX_POINT);
		else if (m_bCircleValid)
			strStatus.Format(_T("정원 중심 (%.1f, %.1f), 반지름 %.1f  —  점을 드래그해 옮길 수 있습니다."),
			                 m_dCircleX, m_dCircleY, m_dCircleR);
		else
			strStatus = _T("세 점이 한 직선 위에 있어 정원을 그릴 수 없습니다.");
	}

	SetDlgItemText(IDC_STATIC_P1, strPoint[0]);
	SetDlgItemText(IDC_STATIC_P2, strPoint[1]);
	SetDlgItemText(IDC_STATIC_P3, strPoint[2]);
	SetDlgItemText(IDC_STATIC_STATUS, strStatus);
}


// ─────────────────────────────────────────────────────────────
// 이미지 그리기 (모두 m_mtxImage 를 잡은 상태에서 호출)
// ─────────────────────────────────────────────────────────────

void CCircleDrawerDlg::initImage()
{
	int nWidth = m_rcView.Width();
	int nHeight = m_rcView.Height();
	int nBpp = 8;

	// 높이를 음수로 주면 top-down 비트맵: 메모리의 첫 행이 화면의 맨 위 행이 된다.
	m_image.Create(nWidth, -nHeight, nBpp);
	if (nBpp == 8) {
		// 8비트 이미지는 팔레트가 필요하다. 0~255 를 같은 밝기의 회색으로 매핑한다.
		static RGBQUAD rgb[256];
		for (int i = 0; i < 256; i++)
			rgb[i].rgbRed = rgb[i].rgbGreen = rgb[i].rgbBlue = (BYTE)i;
		m_image.SetColorTable(0, 256, rgb);
	}
}

void CCircleDrawerDlg::renderImage()
{
	int nHeight = m_image.GetHeight();
	int nPitch = m_image.GetPitch();
	unsigned char* fm = (unsigned char*)m_image.GetBits();

	// 행 끝의 4바이트 정렬 패딩까지 포함해 지우도록 width 대신 pitch 를 쓴다.
	memset(fm, GRAY_BACKGROUND, nPitch * nHeight);

	for (int k = 0; k < m_nClickCount; k++)
		drawCircle(fm, m_ptClick[k].x, m_ptClick[k].y, m_nRadius, GRAY_POINT);

	m_bCircleValid = false;
	if (m_nClickCount == MAX_POINT) {
		m_bCircleValid = calcCircumcircle(m_ptClick, m_dCircleX, m_dCircleY, m_dCircleR);
		if (m_bCircleValid)
			drawRing(fm, m_dCircleX, m_dCircleY, m_dCircleR, m_nThickness, GRAY_CIRCLE);
	}
}

void CCircleDrawerDlg::drawCircle(unsigned char* fm, int nCenterX, int nCenterY, int nRadius, int nGray)
{
	int nPitch = m_image.GetPitch();

	// 원을 감싸는 사각형만 훑으며, 원 안에 있는 픽셀만 칠한다.
	for (int j = nCenterY - nRadius; j <= nCenterY + nRadius; j++) {
		for (int i = nCenterX - nRadius; i <= nCenterX + nRadius; i++) {
			if (validImgPos(i, j) && isInCircle(i, j, nCenterX, nCenterY, nRadius))
				fm[j * nPitch + i] = (unsigned char)nGray;
		}
	}
}

void CCircleDrawerDlg::drawRing(unsigned char* fm, double dCenterX, double dCenterY, double dRadius,
                                int nThickness, int nGray)
{
	int nWidth = m_image.GetWidth();
	int nHeight = m_image.GetHeight();
	int nPitch = m_image.GetPitch();

	// 테두리는 반지름 ± 두께/2 사이의 띠(ring)로 표현한다. 안쪽은 칠하지 않는다.
	double dInner = (std::max)(0.0, dRadius - nThickness / 2.0);
	double dOuter = dRadius + nThickness / 2.0;

	// 외접원은 매우 클 수 있으므로 훑는 범위를 이미지 안으로 제한한다.
	int nLeft = (std::max)(0, (int)std::floor(dCenterX - dOuter));
	int nTop = (std::max)(0, (int)std::floor(dCenterY - dOuter));
	int nRight = (std::min)(nWidth - 1, (int)std::ceil(dCenterX + dOuter));
	int nBottom = (std::min)(nHeight - 1, (int)std::ceil(dCenterY + dOuter));

	for (int j = nTop; j <= nBottom; j++) {
		for (int i = nLeft; i <= nRight; i++) {
			if (isInRing(i, j, dCenterX, dCenterY, dInner, dOuter))
				fm[j * nPitch + i] = (unsigned char)nGray;
		}
	}
}

bool CCircleDrawerDlg::isInCircle(int i, int j, int nCenterX, int nCenterY, int nRadius) const
{
	bool bRet = false;

	double dX = i - nCenterX;
	double dY = j - nCenterY;
	double dDist = dX * dX + dY * dY;  // 제곱근을 피하려고 거리의 제곱끼리 비교한다.

	if (dDist <= nRadius * nRadius)
		bRet = true;

	return bRet;
}

bool CCircleDrawerDlg::isInRing(int i, int j, double dCenterX, double dCenterY,
                                double dInner, double dOuter) const
{
	double dX = i - dCenterX;
	double dY = j - dCenterY;
	double dDist = dX * dX + dY * dY;

	return dDist >= dInner * dInner && dDist <= dOuter * dOuter;
}

BOOL CCircleDrawerDlg::validImgPos(int x, int y) const
{
	int nWidth = m_image.GetWidth();
	int nHeight = m_image.GetHeight();

	CRect rect(0, 0, nWidth, nHeight);
	return rect.PtInRect(CPoint(x, y));
}

bool CCircleDrawerDlg::calcCircumcircle(const CPoint pt[MAX_POINT],
                                        double& dCenterX, double& dCenterY, double& dRadius)
{
	// 세 점 A, B, C 에서 같은 거리에 있는 점(외심)을 구한다.
	//   |P-A|² = |P-B|² = |P-C|² 를 전개하면 중심 (x, y) 에 대한 1차 연립방정식이 된다.
	double ax = pt[0].x, ay = pt[0].y;
	double bx = pt[1].x, by = pt[1].y;
	double cx = pt[2].x, cy = pt[2].y;

	double d = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
	if (std::fabs(d) < 1e-9)
		return false;  // 세 점이 한 직선 위(또는 겹침) → 원이 존재하지 않는다.

	double a2 = ax * ax + ay * ay;
	double b2 = bx * bx + by * by;
	double c2 = cx * cx + cy * cy;

	dCenterX = (a2 * (by - cy) + b2 * (cy - ay) + c2 * (ay - by)) / d;
	dCenterY = (a2 * (cx - bx) + b2 * (ax - cx) + c2 * (bx - ax)) / d;
	dRadius = std::hypot(ax - dCenterX, ay - dCenterY);
	return true;
}


// ─────────────────────────────────────────────────────────────
// 마우스 입력
// ─────────────────────────────────────────────────────────────

CPoint CCircleDrawerDlg::toImagePos(CPoint ptClient) const
{
	return ptClient - m_rcView.TopLeft();
}

int CCircleDrawerDlg::hitTestPoint(CPoint ptImg) const
{
	// 너무 작은 원도 잡을 수 있도록 최소 5픽셀의 여유를 둔다.
	int nHit = (std::max)(m_nRadius, 5);
	for (int k = 0; k < m_nClickCount; k++) {
		CPoint d = ptImg - m_ptClick[k];
		if (d.x * d.x + d.y * d.y <= nHit * nHit)
			return k;
	}
	return -1;
}

void CCircleDrawerDlg::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (!m_rcView.PtInRect(point)) {
		CDialogEx::OnLButtonDown(nFlags, point);
		return;
	}

	CPoint ptImg = toImagePos(point);

	// m_nClickCount 는 UI 쓰레드만 바꾸므로 여기서는 락 없이 읽어도 된다.
	if (m_nClickCount < MAX_POINT) {
		// 1~3번째 클릭: 클릭 지점 원 추가
		// readInputs 는 메시지 박스를 띄울 수 있다. 메시지 박스가 도는 동안 OnPaint 가
		// 같은 쓰레드에서 락을 다시 잡으므로, 입력은 반드시 락을 잡기 전에 읽는다.
		int nRadius = 0;
		int nThickness = 0;
		if (!readInputs(nRadius, nThickness))
			return;

		{
			std::lock_guard<std::mutex> lock(m_mtxImage);
			m_nRadius = nRadius;
			m_nThickness = nThickness;
			m_ptClick[m_nClickCount++] = ptImg;
			renderImage();
		}
		refreshView();
		updateLabels();
		return;
	}

	// 4번째 클릭부터: 새 원은 그리지 않고, 기존 점 위라면 드래그를 시작한다.
	{
		std::lock_guard<std::mutex> lock(m_mtxImage);
		m_nDragIndex = hitTestPoint(ptImg);
	}
	if (m_nDragIndex >= 0)
		SetCapture();  // 커서가 창 밖으로 나가도 마우스 메시지를 계속 받는다.
}

void CCircleDrawerDlg::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_nDragIndex < 0) {
		CDialogEx::OnMouseMove(nFlags, point);
		return;
	}

	{
		std::lock_guard<std::mutex> lock(m_mtxImage);

		// 점이 이미지 밖으로 나가지 않도록 좌표를 영역 안으로 고정한다.
		CPoint ptImg = toImagePos(point);
		ptImg.x = std::clamp((int)ptImg.x, 0, m_image.GetWidth() - 1);
		ptImg.y = std::clamp((int)ptImg.y, 0, m_image.GetHeight() - 1);

		m_ptClick[m_nDragIndex] = ptImg;
		renderImage();
	}

	// 드래그 중에는 즉시 다시 그려야 정원이 커서를 따라 움직이는 것처럼 보인다.
	refreshView();
	UpdateWindow();
	updateLabels();
}

void CCircleDrawerDlg::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_nDragIndex >= 0) {
		m_nDragIndex = -1;
		ReleaseCapture();
	}
	CDialogEx::OnLButtonUp(nFlags, point);
}

bool CCircleDrawerDlg::readInputs(int& nRadius, int& nThickness)
{
	BOOL bRadiusOk = FALSE;
	BOOL bThicknessOk = FALSE;
	nRadius = (int)GetDlgItemInt(IDC_EDIT_RADIUS, &bRadiusOk, FALSE);
	nThickness = (int)GetDlgItemInt(IDC_EDIT_THICKNESS, &bThicknessOk, FALSE);

	if (!bRadiusOk || nRadius < MIN_RADIUS || nRadius > MAX_RADIUS) {
		CString strMsg;
		strMsg.Format(_T("클릭 원 반지름은 %d ~ %d 사이로 입력하세요."), MIN_RADIUS, MAX_RADIUS);
		AfxMessageBox(strMsg);
		return false;
	}
	if (!bThicknessOk || nThickness < MIN_THICKNESS || nThickness > MAX_THICKNESS) {
		CString strMsg;
		strMsg.Format(_T("정원 두께는 %d ~ %d 사이로 입력하세요."), MIN_THICKNESS, MAX_THICKNESS);
		AfxMessageBox(strMsg);
		return false;
	}
	return true;
}


// ─────────────────────────────────────────────────────────────
// 버튼
// ─────────────────────────────────────────────────────────────

void CCircleDrawerDlg::OnBnClickedBtnReset()
{
	stopRandomMove();
	GetDlgItem(IDC_BTN_RANDOM)->EnableWindow(TRUE);

	{
		std::lock_guard<std::mutex> lock(m_mtxImage);
		m_nClickCount = 0;
		m_nDragIndex = -1;
		renderImage();
	}
	refreshView();
	updateLabels();
}

void CCircleDrawerDlg::OnBnClickedBtnRandom()
{
	if (m_threadRandom.joinable())
		return;  // 이미 실행 중

	// 메시지 박스가 뜰 수 있는 확인은 모두 락 밖에서 한다. (OnLButtonDown 주석 참고)
	if (m_nClickCount < MAX_POINT) {
		AfxMessageBox(_T("먼저 그리기 영역을 세 번 클릭해 정원을 그려 주세요."));
		return;
	}
	int nRadius = 0;
	int nThickness = 0;
	if (!readInputs(nRadius, nThickness))
		return;

	{
		std::lock_guard<std::mutex> lock(m_mtxImage);
		m_nRadius = nRadius;
		m_nThickness = nThickness;
	}

	GetDlgItem(IDC_BTN_RANDOM)->EnableWindow(FALSE);
	m_bStopRandom = false;
	m_threadRandom = std::thread(&CCircleDrawerDlg::randomMoveThread, this);
}


// ─────────────────────────────────────────────────────────────
// 랜덤 이동 쓰레드
// ─────────────────────────────────────────────────────────────

void CCircleDrawerDlg::randomMoveThread()
{
	std::mt19937 rng(std::random_device{}());
	auto tNext = std::chrono::steady_clock::now();

	for (int n = 0; n < RANDOM_MOVE_COUNT && !m_bStopRandom; n++) {
		// 다음 실행 시각을 미리 정해 두면 그리기에 걸린 시간만큼 주기가 밀리지 않는다.
		tNext += std::chrono::milliseconds(RANDOM_MOVE_INTERVAL_MS);

		{
			std::lock_guard<std::mutex> lock(m_mtxImage);

			// 클릭 지점 원이 잘리지 않도록 반지름만큼 안쪽에서 위치를 고른다.
			int nMaxX = (std::max)(m_nRadius, m_image.GetWidth() - 1 - m_nRadius);
			int nMaxY = (std::max)(m_nRadius, m_image.GetHeight() - 1 - m_nRadius);
			std::uniform_int_distribution<int> distX(m_nRadius, nMaxX);
			std::uniform_int_distribution<int> distY(m_nRadius, nMaxY);

			for (int k = 0; k < MAX_POINT; k++)
				m_ptClick[k] = CPoint(distX(rng), distY(rng));

			// 이미지 메모리 갱신(그리기)까지 작업 쓰레드에서 수행한다.
			renderImage();
		}

		// 화면 갱신(윈도우 조작)은 UI 쓰레드에서 해야 하므로 메시지로 요청만 한다.
		// SendMessage 는 UI 쓰레드가 join 대기 중일 때 교착될 수 있어 PostMessage 를 쓴다.
		PostMessage(WM_USER_REDRAW);

		// 다음 실행 시각까지 대기. 초기화/종료 요청에 빨리 반응하도록 짧게 나누어 잔다.
		while (!m_bStopRandom && std::chrono::steady_clock::now() < tNext)
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	PostMessage(WM_USER_RANDOM_DONE);
}

void CCircleDrawerDlg::stopRandomMove()
{
	m_bStopRandom = true;
	if (m_threadRandom.joinable())
		m_threadRandom.join();
	m_bStopRandom = false;
}

LRESULT CCircleDrawerDlg::OnRedrawImage(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	refreshView();
	updateLabels();
	return 0;
}

LRESULT CCircleDrawerDlg::OnRandomMoveDone(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	// 쓰레드 함수가 이미 끝났으므로 join 은 즉시 반환된다.
	if (m_threadRandom.joinable())
		m_threadRandom.join();
	GetDlgItem(IDC_BTN_RANDOM)->EnableWindow(TRUE);
	return 0;
}
