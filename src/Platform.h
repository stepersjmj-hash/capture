#pragma once

// 플랫폼별 네이티브 도우미. macOS 구현은 Platform.mm (Objective-C++), Windows 는 각 .cpp 안의 #ifdef.
#include <QtGlobal>

class QWidget;

namespace Platform {

#ifdef Q_OS_MACOS
// NSPasteboard changeCount — 외부 앱(시스템 캡처 등)이 붙여넣기판을 바꿨는지 감지
quint64 pasteboardChangeCount();
// 캡처 오버레이를 메뉴 막대·Dock 위까지 덮도록 창 레벨을 올린다 (모든 Space 에서 표시)
void raiseOverlay(QWidget *w);
// Carbon RegisterEventHotKey — 성공하면 true. id 로 콜백이 구분된다.
bool registerHotKey(int id, int qtKey, int qtModifiers, void **ref);
void unregisterHotKey(void *ref);
void installHotKeyHandler(void (*callback)(int id));
// 가상 키코드(물리 키, 미국 배열 기준) → Qt::Key. 한글 등 라틴이 아닌 입력 소스에서는 키 이벤트의
// key() 가 자모로 오므로, 글자 단축키는 이걸로 한 번 더 본다. 모르는 키면 0.
int keyForVirtualKey(quint32 vk);
// 로그인 시 자동 실행 — ~/Library/LaunchAgents/com.stepersjmj.mcapture.plist (RunAtLoad) 쓰기/지우기.
// 켜져 있으면 실행 파일 경로가 바뀌었을 때(앱을 옮긴 경우) plist 를 다시 쓴다.
bool loginItemEnabled();
void setLoginItem(bool on);
#endif

} // namespace Platform
