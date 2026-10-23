# CircleDrawer — 세 점을 지나는 원 그리기 (MFC 심화 과제)

MFC Dialog 기반 프로그램입니다. 클릭한 세 점을 지나는 정원을 그리고, 점을 드래그하거나 랜덤으로 이동시키면 정원을 다시 그립니다.

## 빌드 / 실행

- Visual Studio 2022 (v143), C++17, MFC (공유 DLL)
- `CircleDrawer.sln` 을 열고 `Release | x64` 로 빌드한 뒤 실행합니다.

```powershell
msbuild CircleDrawer.sln /p:Configuration=Release /p:Platform=x64
.\x64\Release\CircleDrawer.exe
```

## 사용 방법

1. **클릭 원 반지름**과 **정원 두께**를 입력합니다. (기본값 10, 3)
2. 흰색 그리기 영역을 세 번 클릭합니다. 클릭한 위치마다 회색 원이 그려지고, 위쪽에 각 원의 중심 좌표가 표시됩니다.
3. 세 번째 클릭 후 세 점을 모두 지나는 정원이 그려집니다. 안쪽은 비어 있고 테두리 두께는 입력값을 따릅니다.
4. 네 번째 클릭부터는 새 원이 그려지지 않습니다. 대신 기존 점을 클릭한 채 드래그하면, 마우스를 놓을 때까지 정원이 계속 따라 움직이며 다시 그려집니다.
5. **[랜덤 이동]**: 세 점을 무작위 위치로 옮기고 정원을 다시 그리는 동작을 0.5초마다(초당 2회) 총 10번 반복합니다. 별도 쓰레드에서 실행되어 그동안에도 창이 멈추지 않습니다.
6. **[초기화]**: 그린 내용을 모두 지우고 처음 상태로 돌아갑니다. 랜덤 이동 도중에 눌러도 즉시 멈춥니다.

## 구현 요약

| 요구사항 | 구현 |
| --- | --- |
| 원 그리기 방식 | `CImage`(8비트 회색조)의 픽셀 메모리(`GetBits`, `GetPitch`)에 직접 밝기 값을 기록. `Ellipse`, GDI+, `FillPolygon`, `DrawPolygon` 미사용 |
| 클릭 지점 원 | `drawCircle` + `isInCircle`: 원을 감싸는 사각형을 훑으며 중심까지 거리² ≤ 반지름² 인 픽셀을 칠함 |
| 세 점을 지나는 정원 | `calcCircumcircle` 로 외심과 반지름을 구하고, `drawRing` + `isInRing` 으로 `반지름 ± 두께/2` 사이의 픽셀만 칠함 (내부 비움) |
| 그리기 영역 밖 | `validImgPos` 와 범위 제한으로 이미지 밖 픽셀은 쓰지 않음 (원이 잘려서 보일 수 있음) |
| 드래그 | `WM_LBUTTONDOWN` 에서 점을 찾아 `SetCapture`, `WM_MOUSEMOVE` 마다 다시 그림, `WM_LBUTTONUP` 에서 해제 |
| 랜덤 이동 쓰레드 | `std::thread` 가 점 이동과 이미지 그리기를 수행하고, `PostMessage` 로 UI 쓰레드에 화면 갱신만 요청 |
| 쓰레드 안전 | 점 좌표와 이미지는 `std::mutex` 로 보호. 초기화/종료 시 중지 플래그(`std::atomic<bool>`)를 세우고 `join` |
| 예외 상황 | 세 점이 한 직선 위에 있으면 정원을 그리지 않고 안내 문구 표시. 반지름(1~100), 두께(1~50) 입력 검증 |

## 파일 구성

| 파일 | 설명 |
| --- | --- |
| `CircleDrawerDlg.h/.cpp` | 메인 대화 상자: 그리기, 마우스 처리, 랜덤 이동 쓰레드 |
| `CircleDrawer.h/.cpp` | 응용 프로그램 클래스 (대화 상자 실행) |
| `CircleDrawer.rc`, `resource.h` | 대화 상자 레이아웃과 컨트롤 ID |
| `pch.h/.cpp`, `targetver.h` | 미리 컴파일된 헤더 |
