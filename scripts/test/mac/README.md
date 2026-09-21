# macOS UI 검증 도구

자리에 없어도 편집 창·단축키 동작을 확인하는 데 쓴 스크립트 (Claude 세션용). 모두 **손쉬운 접근** 권한이
있는 터미널(또는 그 부모 앱)에서 돌려야 한다. `osascript` 의 `keystroke` 는 Qt 창에 안 들어가므로 키 입력은
반드시 CGEvent(`input`)로 보낸다.

```bash
cd scripts/test/mac && swiftc -o input input.swift && swiftc -o is is.swift
./input key 3 ctrl shift cmd          # ⌃⇧⌘F — 전체 화면 캡처 → 편집 창
./input drag 300 300 620 520          # 영역 캡처 오버레이에서 드래그 (전역 좌표, 포인트)
./input key 46                        # M — 사각형 도구
osascript -e 'tell application "System Events" to tell process "Mcapture" to get value of checkboxes 1 thru 7 of window 1'
                                      # 툴바 도구 체크 상태 (선택,영역,사각형,밑줄,화살표,텍스트,채우기)
lsappinfo info -only name "$(lsappinfo front)"   # 앞에 있는 앱 (System Events 의 frontmost 는 LSUIElement 앱에 부정확)
./is get; ./is set com.apple.keylayout.ABC        # 입력 소스 — 새 편집 창을 열어야 반영된다
```

- 실행 중인 인스턴스 바꾸기: `build-mac/Mcapture.app/Contents/MacOS/Mcapture --quit` (IPC 로 종료) 후 새 앱 실행.
- 진단 로그: `$TMPDIR/Mcapture.log` (`hotkey fired`, `overlay press/release`, `openEditor`, `tool key via virtual key`).
