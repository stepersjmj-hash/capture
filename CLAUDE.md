# Mcapture — Claude 세션 안내서

트레이 상주형 **화면 캡처 + 간단 편집** 도구 (Qt 6 Widgets, C++17, Windows · macOS 단일 코드베이스 —
mplayer 와 같은 구성). **수집·전송·텔레메트리 없음** 을 지킨다 (온라인 API 추가는 사용자 결정 없이는
금지). 네트워크는 **내 NAS 에서 새 버전을 읽어 오는 자동 업데이트**(HTTPS GET, `src/Updater.cpp`) 하나뿐.
저장소 이름은 `capture`, 앱·실행 파일 이름은 `Mcapture` (M 시리즈: Mview·Mzip·Mplayer).

## 구조

```
CMakeLists.txt        project(Mcapture VERSION x.y.z) — 버전은 여기 한 곳 (APP_VERSION 으로 전 소스에 주입)
src/main.cpp          단일 인스턴스(QLocalServer "McaptureSingleInstance"), 설정 파일 위치, 스타일, --export-ico/icns
src/App.*             트레이·메뉴, 전역 단축키 연결, 클립보드 감시 연결, 편집 창 열기, 명령줄, 업데이트 알림
src/Defaults.h        기본 단축키·저장 폴더·OS 캡처 키 표기
src/HotKey.*          전역 단축키 (Win: RegisterHotKey(NULL)+네이티브 필터 / mac: Carbon, Platform.mm)
src/ClipboardWatch.*  클립보드 감시 (시퀀스 폴링 + 디바운스 350ms + 같은 이미지 중복 억제)
src/CaptureOverlay.*  화면별 영역 선택 오버레이(CaptureOverlay) + 세션(RegionCapture) + 전체 화면 grab
src/EditorWindow.*    편집 창 (툴바·상태바·단축키·복사/저장/닫기 확인)
src/Canvas.*          캔버스 (배율 맞춤, 영역 선택 마퀴·자르기, 그리기/선택/이동, 텍스트 인라인 입력,
                      우클릭 메뉴, 되돌리기 100단계 — 이미지까지 한 칸(Snapshot)이라 자르기도 되돌아간다)
src/Annotation.h      항목 모델(Item) + 그리기/히트테스트 (이미지 픽셀 좌표 기준). Tool::Region 은
                      화면 위 선택 영역만 다루는 도구라 Item 으로 저장되지 않는다
src/HelpDialog.*      단축키 치트시트 (F1) — Mview 단축키 오버레이를 옮긴 반투명 모달
src/SettingsDialog.*  설정 대화상자 (QSettings 에 쓰기만; 적용은 App::applySettings)
src/Updater.*         NAS 자동 업데이트 (mplayer 에서 이식, 이름만 변경)
src/Theme.h / Icons.h 디자인 토큰(Mview 차콜+앰버) QSS / Material Icons 글리프 → QIcon
src/Platform.h/.mm    macOS 전용 네이티브 (pasteboard changeCount, 오버레이 창 레벨, Carbon 단축키) — Win 빌드 제외
assets/               app.svg(아이콘 마스터) → app.ico/app.icns(생성물, 추적됨), fonts/(Material Icons Outlined)
scripts/test/         UI 자동 검증용 PowerShell (winshot·mouse·keys·winmove — 아래 "화면 검증")
make-dist.ps1 / make-dist-mac.sh / release.ps1 / release-mac.sh / .github/workflows/build.yml — mplayer 와 동일 구조
```

설정 파일: Windows `build\Mcapture.ini`(실행 파일 옆, 포터블) / mac `~/Library/Application Support/Mcapture/Mcapture.ini`.
키: `hotkey/region` `hotkey/full`(PortableText) `clipboard/watch` `capture/autoCopy` `save/dir` `startup/run`
`update/auto` `update/url` `edit/color|width|textPx` `app/firstRunShown`.

## 빌드 (Windows / MSYS2 MINGW64)

```powershell
$env:MSYSTEM = "MINGW64"
C:\msys64\usr\bin\bash.exe -lc "cd /c/Users/stepe/Desktop/mj/capture && cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build -j"
```
- 필요 패키지: `mingw-w64-x86_64-{gcc,cmake,ninja,qt6-base,qt6-svg}` (Qt 6.11 로 검증). 제너레이터는 Ninja.
- 실행 중인 Mcapture.exe 가 있으면 링크 실패 → `Stop-Process -Name Mcapture` 먼저.
- 아이콘: `build/Mcapture.exe --export-ico assets/app.svg assets/app.ico` (icns 도 같은 식) 후 **재구성**
  (`cmake -B build …` 다시 — app.rc 포함 여부를 configure 때 결정). 이미 생성돼 추적 중이므로 SVG 를
  바꿨을 때만 다시 만든다.
- 테스트·린트는 없다. 검증은 빌드 + 실행 + 화면 확인.

### 실행 (DLL 배포, 최초 1회 / Qt 패키지 갱신 후)

```bash
windeployqt6 --release --no-translations build/Mcapture.exe
for dll in $(ldd build/Mcapture.exe | grep -i mingw64 | awk '{print $3}' | sort -u); do [ -e build/$(basename $dll) ] || cp $dll build/; done
mkdir -p build/tls && cp /mingw64/share/qt6/plugins/tls/qschannelbackend.dll build/tls/
```
**함정: tls 플러그인** — windeployqt 가 넣어 주지 않아 없으면 HTTPS(자동 업데이트)가 조용히 실패한다.
make-dist.ps1 은 없으면 오류를 낸다.

### 화면 검증 (자리에 없어도 확인 가능 — v1.0.0 에서 실제로 쓴 흐름)

`scripts/test/*.ps1` 을 **Windows PowerShell 5.1**(`powershell -ExecutionPolicy Bypass -File …`)로 돌린다
(pwsh 7 에는 System.Drawing 이 없음).
1. `Start-Process build\Mcapture.exe` → 트레이 상주. `build\Mcapture.exe --region` 을 **두 번째로 실행**하면
   IPC 로 기존 인스턴스가 오버레이를 띄운다 — 뜨기까지 1~2초 걸리므로 **드래그 전 3~4초 대기**
   (1.5초는 부족해 드래그 절반만 잡혔다). `--full` 은 즉시 편집 창.
2. `mouse.ps1 drag x1 y1 x2 y2` (화면 절대 좌표) 로 영역 선택 → 편집 창 제목 `Mcapture — W × H`.
3. `winmove.ps1 "W × H" 100 100` 으로 편집 창을 고정 위치로 옮긴 뒤 툴바 클릭·드래그 좌표를 계산한다
   (창 1363×566 기준 툴바 y=+58, 캔버스 이미지는 창 중앙). **제목 부분 일치라 창이 여러 개면 엉뚱한 창을
   잡는다** — 크기 문자열(`601 × 401`)로 지정하고, 안 쓰는 창은 `winmove.ps1 <제목> 0 0 close` 로 닫을 것.
4. `keys.ps1 "^s"` 등 SendKeys 는 별도 프로세스라 포그라운드가 안 바뀐다. `winshot.ps1 <제목> out.png` 가
   창 영역만 캡처 (`=정확한제목` 으로 완전 일치 — 닫기 확인 상자 제목이 `Mcapture` 라 부분 일치로는 편집 창이
   먼저 잡힌다).
5. 클립보드 감시 검증: `powershell -STA -Command "[System.Windows.Forms.Clipboard]::SetImage(비트맵)"` →
   3초 안에 `Mcapture — 300 × 200` 창이 **하나만** 떠야 한다.
6. 검증 스크린샷은 사용자 화면 내용이 찍히므로 scratchpad 에만 두고 끝나면 지운다.

### 배포

`make-dist.ps1` → `dist/Mcapture/` + `dist/Mcapture.zip` (exe + DLL + platforms/styles/iconengines/imageformats/tls).

## 릴리즈 절차 (버전업 때마다)

1. `CMakeLists.txt` `project(Mcapture VERSION x.y.z)` 수정.
2. `CHANGELOG.md` 항목 추가, `README.md`(사용법·단축키), 이 파일(구조·함정) 갱신.
3. 빌드 → 실행 확인 → `make-dist.ps1`.
4. 커밋(`v<버전> <요약>`) + 푸시 → `pwsh -File release.ps1 [-SkipMac]` — zip 을 `Mcapture-<v>-win.zip` 으로
   태그 + GitHub Release + **NAS 업로드**(`\\mjj\web\site\mjimage\MJ_data\Mcapture` 에 zip + `version.txt`).
   태그 푸시로 CI 가 win zip·mac dmg·mac zip 을 릴리스에 첨부하고, release.ps1 이 CI 를 기다려 mac zip 을
   NAS 에 `version-mac.txt` 와 함께 올린다 (`-SkipMac` 이면 생략, 나중에 재실행하면 이어서 함).
   Updater 의 기본 URL 은 `https://stepersjmj.synology.me:28443/mjimage/MJ_data/Mcapture` (`update/url` 로 재지정 가능).
5. 옵시디언 일지(`_generate.ps1`)·스펙 문서(`프로젝트/capture 스펙.md`) 갱신 (전역 CLAUDE.md 규칙).

## 설계 메모·함정

- **캡처 흐름**: `RegionCapture::begin` 이 먼저 **모든 화면을 grabWindow(0)** 한 뒤 화면마다
  `CaptureOverlay`(Qt::Tool | FramelessWindowHint | StaysOnTop, 화면 geometry) 를 띄운다 — 오버레이가 다른
  화면 스크린샷에 찍히지 않게. 선택은 논리 좌표, 자르기는 `devicePixelRatio` 를 곱한 픽셀 좌표
  (`Qt::HighDpiScaleFactorRoundingPolicy::PassThrough`). 선택 크기는 끌어간 거리 그대로
  (`QRect(p1,p2)` 는 +1px 라 직접 계산). 메뉴에서 부를 때는 200ms 지연(메뉴가 닫힌 뒤 찍게).
- **클립보드 감시**: 앱들이 포맷을 여러 번 나눠 넣어 `GetClipboardSequenceNumber` 가 연달아 바뀐다 —
  디바운스 없이 읽으면 같은 이미지로 편집 창이 두 개 떴다(실측). 350ms 잠잠해진 뒤 한 번 읽고, 직전 이미지와
  같으면 무시. 우리가 넣은 복사는 `ignoreCurrent()` 로 기준점 갱신. 텍스트/HTML/URL 이 함께 있으면
  캡처가 아니라 보고 무시(브라우저 이미지 복사 등). mac 은 dataChanged 가 외부 변경에 안 오므로 500ms 폴링.
- **Win+Shift+S 는 클립보드에 두 번 넣는다** (사용자 피드백 "캡처하면 창이 2개 — 전체와 선택"): 스니핑
  도구는 시작할 때 전체 화면(모니터/가상 데스크톱 픽셀 크기)을, 영역을 고르면 그 결과를 잇달아 클립보드에 넣는다.
  `App::onClipboardImage`: 화면 크기와 같은 이미지는 8초 보류(`m_pendingClip`) → 그 안에 다른 이미지가 오면
  버림, 안 오면 열기. 또 직전에 클립보드로 연 창이 20초 안이고 편집 전이면 새 창 대신 `replaceImage`.
  진단은 `%TEMP%\Mcapture.log`(mlog — 이벤트·크기만 기록, 512KB 넘으면 새로 시작).
- **전역 단축키(Win)**: `RegisterHotKey(NULL, id, MOD_NOREPEAT|…)` → WM_HOTKEY 가 스레드 메시지(hwnd 0)로
  오고 Qt 디스패처가 네이티브 필터에 넘긴다. 다른 앱이 쓰는 조합은 등록 실패 → 트레이 알림/설정 경고.
- **오버레이 더블클릭 금지**: 클릭 직후 바로 드래그하면 Qt 가 두 번째 누름에 Press 와 DblClick 을 둘 다 보낸다
  (QGuiApplicationPrivate::processMouseEvent). 더블클릭 = 전체 화면으로 두었더니 "전체 + 선택 영역" 창이 함께
  열렸다(사용자 피드백) → 더블클릭 핸들러 제거 + `m_done` 으로 오버레이당 결과 1회. 검증: `mouse.ps1 click` 직후
  `drag` → 창 1개.
- **선택 영역(Region) = 기본 도구**: Mview 자르기 모드를 그대로 옮겼다 — 바깥 딤 rgba(8,8,10,166),
  60ms 마다 행진하는 흰 점선(m_dashPhase), 코너 손잡이 10px, 실시간 `W × H px` 배지, Shift 정사각형,
  안쪽 드래그로 영역 이동, Enter 자르기, Esc 선택 지우기. 도구를 바꾸면 영역은 지워진다.
  자르기는 `m_img` 를 바꾸고 항목 좌표를 `-topLeft` 만큼 옮긴다(항목은 계속 편집 가능) —
  되돌리기 한 칸이 `{items, img, imageEdited}` 라 `Ctrl+Z` 로 자르기 전으로 돌아간다. QImage 는
  암묵적 공유라 이미지가 안 바뀐 단계는 사본을 만들지 않는다. 크기가 바뀌면 `imageResized` 로
  창 제목·상태바·창 크기를 다시 맞춘다(`fitToImage(size, keepPos=true)` — 위치는 유지).
  **자르기만 하고 항목이 없어도 "편집됨"** 이어야 하므로 닫기 확인은 `hasItems()` 가 아니라
  `hasContent()`(= 항목 있음 ‖ imageEdited) 를 본다.
- **우클릭 메뉴 세 가지**: 항목 위 → 항목 메뉴(Canvas), 선택 영역 있음 → 영역 메뉴(Canvas),
  빈 곳 → `Canvas::menuRequested` 로 편집 창이 띄우는 도구·도움말 메뉴
  (전체 선택 · 도구 6개 · 단축키 보기 · 업데이트 확인 · 버전 정보 — 뒤 둘은 신호로 App 에 넘긴다).
  Windows 는 WM_CONTEXTMENU 가 **버튼을 뗄 때** 오므로, 누를 때 `cancelPending()`
  으로 선택/영역을 지우면 메뉴가 뜰 대상이 사라진다 — `mousePressEvent` 의 오른쪽 버튼은 그냥
  돌려보내고 판단은 전부 `contextMenuEvent` 에서 한다 (항목 위 → 항목 메뉴 / 영역 있음 → 영역 메뉴 /
  그리는 중·입력 중 → 취소). 메뉴 단축키 표기는 `QAction::setShortcut` 대신 `"...\tEnter"` —
  setShortcut 은 `Return`·`Del` 로 찍혀 Mview 말투와 어긋난다.
- **텍스트 테두리**: 기본이 **없음**(`Item::outline=false`). 확정한 텍스트를 우클릭 → `테두리` 로
  켜고, 그 값이 `m_textOutline` 에 남아 다음에 넣는 텍스트에도 이어진다.
- **텍스트 크기**: 확정(Enter)하면 그 항목을 **선택 상태**로 둔다(Text 도구에서도 선택 유지) — 글자 스핀·휠(10%)·
  오른쪽 아래 손잡이 드래그(폭 비율로 px 환산, Mview 방식)가 바로 적용된다. 선택하면 항목의 색·굵기·글자 크기를
  캔버스 현재값으로 가져오고 `colorChanged/lineWidthChanged/textPxChanged` 로 툴바를 맞춘다 (스핀은
  QSignalBlocker 로 되먹임 차단).
- **편집 창 키**: 툴바 버튼은 `Qt::NoFocus` 라 키 입력이 캔버스로 간다. 도구 단축키(선택 Space · 영역 C ·
  사각형 M · 밑줄 U · 화살표 Shift+. · 텍스트 T · 채우기 F, Ctrl+A = 영역 도구 + 이미지 전체 선택)는 QAction
  (QLineEdit 텍스트 입력 중에는 QLineEdit 가 ShortcutOverride 로 가로채므로 글자가 그대로 입력된다 —
  Space·글자·`>` 모두 `Qt::Key_Escape` 보다 작은 키코드라 QLineEdit 이 먼저 가져간다).
  **함정: 한 동작에 같은 이벤트로 맞는 조합을 여러 개 걸면 안 된다** — 화살표에 `Shift+.` 과 `>` 를 함께
  등록했더니 Qt 가 "모호한 단축키" 로 보고 키는 먹은 채 아무것도 실행하지 않았다(실측). `Shift+.` 하나만 등록.
- **F1 단축키 치트시트**(`HelpDialog`): 부모 창을 덮는 프레임 없는 반투명 모달. **그냥 띄우면 활성 창이
  되지 않아 Esc 가 안 먹는다** — `showEvent` 에서 `raise()+activateWindow()+setFocus()` 를 직접 호출한다.
  패널(760px)이 창보다 크면 창 밖으로 넓혀서 화면 안에 맞춘다 (작은 캡처는 창이 760×440 라 세로가 모자람).
  `Esc` 는 텍스트 취소 → 선택 해제 → 창 닫기 순(`Canvas::cancelPending`).
- **자동 복사 + 감시 충돌**: 캡처 직후 `setImage` 뒤 곧바로 `ignoreCurrent()` — 순서가 바뀌면 자기 캡처를
  다시 연다.
- **macOS 는 이 PC 에서 컴파일 불가** — `Platform.mm`(Carbon 단축키·NSPasteboard changeCount·오버레이
  `NSScreenSaverWindowLevel`), `LSUIElement`(Dock 숨김), 화면 기록 권한 흐름은 CI 컴파일만 거쳤고 실기기
  검증 대기. Qt 는 mac 에서 Ctrl↔Cmd 를 맞바꾸므로 기본 단축키 문자열은 `Meta+Shift+Ctrl+S` (= ⌃⇧⌘S).
- Windows 자동 실행은 `HKCU\…\Run` 의 `Mcapture` 값 (설정 적용 때 쓰거나 지움).

## 현재 상태

- **v1.1.0** (2026-09-21 구현·**릴리스** — GitHub Release v1.1.0 에 win zip·mac dmg·mac zip,
  NAS 에 `version.txt`/`version-mac.txt` 갱신 완료) — 2차 피드백: ① 기본 드래그를 사각 선택 영역으로 바꾸고 우클릭 메뉴
  (자르기·색 채우기·테두리 추가·선택 지우기) ② 텍스트 기본 테두리 제거 + 항목 우클릭 메뉴
  (테두리·색 변경·텍스트 수정·삭제). Mview 자르기 UX 를 그대로 잇는 게 요구사항이었다.
  ③ 단축키 재배치(Space/C/M/U/Shift+./T/F, Ctrl+F 폴더, Ctrl+Shift+Z 다시 실행) + F1 치트시트.
  Windows 실검증(스크린샷): 툴바 기본 도구 `영역`, 마퀴·배지, 영역 메뉴 4개 항목, 테두리 추가,
  Enter 자르기(350×200, 창 위치 유지) → Ctrl+Z 로 600×400 복원, 텍스트 외곽선 없이 입력 → 우클릭
  `테두리` 로 켜기, 색 채우기, Esc 선택 지우기, F1 치트시트 열고 Esc 로 닫기, Space/M/U/Shift+. 도구 전환.

- v1.0.0 (2026-09-20 구현, **2026-09-21 릴리스** — GitHub Release v1.0.0 에 win zip·mac dmg·mac zip, NAS 에 `version.txt`/`version-mac.txt` 업로드 완료, 자동 업데이트 채널 개통) — 첫 구현. 1차 피드백 반영: 텍스트 확정 후 크기 조절(휠·손잡이·스핀),
  클릭 직후 드래그 시 전체+선택 두 창 문제(더블클릭 제거), 자기 복사본 중복 열기 방지(ignoreCurrent 에 이미지 전달),
  Win+Shift+S 의 전체 화면 선행 복사본 보류/교체(진짜 원인 — 로그로 확인). Windows 실검증: 영역/전체 캡처, 5개 편집 도구, 선택·이동,
  되돌리기, 저장(`사진\Mcapture`), 복사(감시 재진입 없음), 외부 클립보드 이미지 → 창 1개, 닫기 확인.
  미검증: macOS 전반(CI 컴파일 확인만), 다중 모니터·고DPI 실기기, 자동 업데이트 E2E(NAS 에 첫 배포 후 확인 가능).
