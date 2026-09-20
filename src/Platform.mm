// Platform.mm — macOS 네이티브 도우미 (Objective-C++). Windows 빌드에는 포함되지 않는다.
#include "Platform.h"

#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>
#include <QWidget>
#include <QWindow>
#include <Qt>

namespace Platform {

quint64 pasteboardChangeCount() {
    return quint64([[NSPasteboard generalPasteboard] changeCount]);
}

void raiseOverlay(QWidget *w) {
    if (!w || !w->windowHandle())
        return;
    NSView *view = reinterpret_cast<NSView *>(w->winId());
    NSWindow *win = [view window];
    if (!win)
        return;
    [win setLevel:NSScreenSaverWindowLevel];
    [win setCollectionBehavior:(NSWindowCollectionBehaviorCanJoinAllSpaces |
                                NSWindowCollectionBehaviorFullScreenAuxiliary |
                                NSWindowCollectionBehaviorStationary)];
    [win makeKeyAndOrderFront:nil];
}

// Qt::Key → macOS 가상 키코드 (미국 배열 기준 — Carbon 단축키는 물리 키 기준)
static int macKeyCode(int key) {
    switch (key) {
    case Qt::Key_A: return kVK_ANSI_A; case Qt::Key_B: return kVK_ANSI_B;
    case Qt::Key_C: return kVK_ANSI_C; case Qt::Key_D: return kVK_ANSI_D;
    case Qt::Key_E: return kVK_ANSI_E; case Qt::Key_F: return kVK_ANSI_F;
    case Qt::Key_G: return kVK_ANSI_G; case Qt::Key_H: return kVK_ANSI_H;
    case Qt::Key_I: return kVK_ANSI_I; case Qt::Key_J: return kVK_ANSI_J;
    case Qt::Key_K: return kVK_ANSI_K; case Qt::Key_L: return kVK_ANSI_L;
    case Qt::Key_M: return kVK_ANSI_M; case Qt::Key_N: return kVK_ANSI_N;
    case Qt::Key_O: return kVK_ANSI_O; case Qt::Key_P: return kVK_ANSI_P;
    case Qt::Key_Q: return kVK_ANSI_Q; case Qt::Key_R: return kVK_ANSI_R;
    case Qt::Key_S: return kVK_ANSI_S; case Qt::Key_T: return kVK_ANSI_T;
    case Qt::Key_U: return kVK_ANSI_U; case Qt::Key_V: return kVK_ANSI_V;
    case Qt::Key_W: return kVK_ANSI_W; case Qt::Key_X: return kVK_ANSI_X;
    case Qt::Key_Y: return kVK_ANSI_Y; case Qt::Key_Z: return kVK_ANSI_Z;
    case Qt::Key_0: return kVK_ANSI_0; case Qt::Key_1: return kVK_ANSI_1;
    case Qt::Key_2: return kVK_ANSI_2; case Qt::Key_3: return kVK_ANSI_3;
    case Qt::Key_4: return kVK_ANSI_4; case Qt::Key_5: return kVK_ANSI_5;
    case Qt::Key_6: return kVK_ANSI_6; case Qt::Key_7: return kVK_ANSI_7;
    case Qt::Key_8: return kVK_ANSI_8; case Qt::Key_9: return kVK_ANSI_9;
    case Qt::Key_F1: return kVK_F1; case Qt::Key_F2: return kVK_F2;
    case Qt::Key_F3: return kVK_F3; case Qt::Key_F4: return kVK_F4;
    case Qt::Key_F5: return kVK_F5; case Qt::Key_F6: return kVK_F6;
    case Qt::Key_F7: return kVK_F7; case Qt::Key_F8: return kVK_F8;
    case Qt::Key_F9: return kVK_F9; case Qt::Key_F10: return kVK_F10;
    case Qt::Key_F11: return kVK_F11; case Qt::Key_F12: return kVK_F12;
    case Qt::Key_Space: return kVK_Space; case Qt::Key_Escape: return kVK_Escape;
    case Qt::Key_Return: return kVK_Return; case Qt::Key_Tab: return kVK_Tab;
    case Qt::Key_Delete: return kVK_ForwardDelete; case Qt::Key_Backspace: return kVK_Delete;
    case Qt::Key_Home: return kVK_Home; case Qt::Key_End: return kVK_End;
    case Qt::Key_PageUp: return kVK_PageUp; case Qt::Key_PageDown: return kVK_PageDown;
    case Qt::Key_Left: return kVK_LeftArrow; case Qt::Key_Right: return kVK_RightArrow;
    case Qt::Key_Up: return kVK_UpArrow; case Qt::Key_Down: return kVK_DownArrow;
    case Qt::Key_Minus: return kVK_ANSI_Minus; case Qt::Key_Equal: return kVK_ANSI_Equal;
    case Qt::Key_BracketLeft: return kVK_ANSI_LeftBracket;
    case Qt::Key_BracketRight: return kVK_ANSI_RightBracket;
    case Qt::Key_Semicolon: return kVK_ANSI_Semicolon;
    case Qt::Key_Apostrophe: return kVK_ANSI_Quote;
    case Qt::Key_Comma: return kVK_ANSI_Comma; case Qt::Key_Period: return kVK_ANSI_Period;
    case Qt::Key_Slash: return kVK_ANSI_Slash; case Qt::Key_Backslash: return kVK_ANSI_Backslash;
    case Qt::Key_QuoteLeft: return kVK_ANSI_Grave;
    default: return -1;
    }
}

static void (*s_callback)(int) = nullptr;
static EventHandlerRef s_handler = nullptr;

static OSStatus hotKeyHandler(EventHandlerCallRef, EventRef event, void *) {
    EventHotKeyID hk{};
    GetEventParameter(event, kEventParamDirectObject, typeEventHotKeyID, nullptr, sizeof(hk),
                      nullptr, &hk);
    if (s_callback)
        s_callback(int(hk.id));
    return noErr;
}

void installHotKeyHandler(void (*callback)(int id)) {
    s_callback = callback;
    if (s_handler)
        return;
    EventTypeSpec spec{kEventClassKeyboard, kEventHotKeyPressed};
    InstallApplicationEventHandler(&hotKeyHandler, 1, &spec, nullptr, &s_handler);
}

bool registerHotKey(int id, int qtKey, int qtModifiers, void **ref) {
    const int code = macKeyCode(qtKey);
    if (code < 0)
        return false;
    UInt32 mods = 0;
    // Qt 는 macOS 에서 Ctrl↔Cmd 를 맞바꾼다: ControlModifier = ⌘, MetaModifier = ⌃
    if (qtModifiers & Qt::ControlModifier) mods |= cmdKey;
    if (qtModifiers & Qt::MetaModifier) mods |= controlKey;
    if (qtModifiers & Qt::AltModifier) mods |= optionKey;
    if (qtModifiers & Qt::ShiftModifier) mods |= shiftKey;
    EventHotKeyID hkId{'MCAP', UInt32(id)};
    EventHotKeyRef hkRef = nullptr;
    const OSStatus st = RegisterEventHotKey(UInt32(code), mods, hkId, GetApplicationEventTarget(),
                                            0, &hkRef);
    if (st != noErr)
        return false;
    *ref = hkRef;
    return true;
}

void unregisterHotKey(void *ref) {
    if (ref)
        UnregisterEventHotKey(static_cast<EventHotKeyRef>(ref));
}

} // namespace Platform
