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
#endif

} // namespace Platform
