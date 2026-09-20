#include "HotKey.h"
#include "Log.h"
#include "Platform.h"

#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QHash>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

int HotKey::s_nextId = 1;

namespace {
QHash<int, HotKey *> &registry() {
    static QHash<int, HotKey *> r;
    return r;
}
} // namespace

struct HotKeyDispatch {
    static void fire(int id) {
        mlog(QString("hotkey fired id=%1").arg(id));
        if (HotKey *h = registry().value(id))
            emit h->activated();
    }
};

#ifdef Q_OS_WIN
namespace {
// RegisterHotKey(NULL, ...) 은 WM_HOTKEY 를 스레드 메시지(hwnd 0)로 보내며,
// Qt 의 Windows 이벤트 디스패처는 그런 메시지도 네이티브 필터에 먼저 넘긴다.
class WinHotKeyFilter : public QAbstractNativeEventFilter {
public:
    bool nativeEventFilter(const QByteArray &type, void *message, qintptr *) override {
        if (type != "windows_generic_MSG")
            return false;
        MSG *msg = static_cast<MSG *>(message);
        if (msg->message == WM_HOTKEY) {
            HotKeyDispatch::fire(int(msg->wParam));
            return true;
        }
        return false;
    }
};

void ensureFilter() {
    static WinHotKeyFilter *filter = nullptr;
    if (!filter) {
        filter = new WinHotKeyFilter;
        QCoreApplication::instance()->installNativeEventFilter(filter);
    }
}

bool toWin(const QKeySequence &seq, UINT &mods, UINT &vk) {
    if (seq.isEmpty())
        return false;
    const QKeyCombination combo = seq[0];
    const Qt::KeyboardModifiers m = combo.keyboardModifiers();
    const int key = combo.key();
    mods = MOD_NOREPEAT;
    if (m & Qt::ControlModifier) mods |= MOD_CONTROL;
    if (m & Qt::ShiftModifier) mods |= MOD_SHIFT;
    if (m & Qt::AltModifier) mods |= MOD_ALT;
    if (m & Qt::MetaModifier) mods |= MOD_WIN;

    if (key >= Qt::Key_A && key <= Qt::Key_Z) vk = UINT(0x41 + (key - Qt::Key_A));
    else if (key >= Qt::Key_0 && key <= Qt::Key_9) vk = UINT(0x30 + (key - Qt::Key_0));
    else if (key >= Qt::Key_F1 && key <= Qt::Key_F24) vk = UINT(VK_F1 + (key - Qt::Key_F1));
    else {
        switch (key) {
        case Qt::Key_Space: vk = VK_SPACE; break;
        case Qt::Key_Print: vk = VK_SNAPSHOT; break;
        case Qt::Key_Insert: vk = VK_INSERT; break;
        case Qt::Key_Delete: vk = VK_DELETE; break;
        case Qt::Key_Home: vk = VK_HOME; break;
        case Qt::Key_End: vk = VK_END; break;
        case Qt::Key_PageUp: vk = VK_PRIOR; break;
        case Qt::Key_PageDown: vk = VK_NEXT; break;
        case Qt::Key_Left: vk = VK_LEFT; break;
        case Qt::Key_Right: vk = VK_RIGHT; break;
        case Qt::Key_Up: vk = VK_UP; break;
        case Qt::Key_Down: vk = VK_DOWN; break;
        case Qt::Key_Escape: vk = VK_ESCAPE; break;
        case Qt::Key_Tab: vk = VK_TAB; break;
        case Qt::Key_Return: case Qt::Key_Enter: vk = VK_RETURN; break;
        case Qt::Key_Backspace: vk = VK_BACK; break;
        case Qt::Key_Pause: vk = VK_PAUSE; break;
        case Qt::Key_ScrollLock: vk = VK_SCROLL; break;
        default:
            if (key > 0 && key < 0x10000) {   // 문장부호 등: 현재 키보드 배열에서 역조회
                const SHORT r = VkKeyScanW(wchar_t(key));
                if (r == -1) return false;
                vk = UINT(r & 0xff);
            } else {
                return false;
            }
        }
    }
    return true;
}
} // namespace
#endif

HotKey::HotKey(QObject *parent) : QObject(parent), m_id(s_nextId++) {
    registry().insert(m_id, this);
#ifdef Q_OS_MACOS
    Platform::installHotKeyHandler(&HotKeyDispatch::fire);
#endif
}

HotKey::~HotKey() {
    unregisterNative();
    registry().remove(m_id);
}

void HotKey::clear() {
    unregisterNative();
    m_seq = QKeySequence();
}

bool HotKey::setSequence(const QKeySequence &seq) {
    unregisterNative();
    m_seq = seq;
    if (seq.isEmpty())
        return true;
    m_registered = registerNative();
    return m_registered;
}

bool HotKey::registerNative() {
#ifdef Q_OS_WIN
    ensureFilter();
    UINT mods = 0, vk = 0;
    if (!toWin(m_seq, mods, vk))
        return false;
    return RegisterHotKey(nullptr, m_id, mods, vk) != 0;
#elif defined(Q_OS_MACOS)
    const QKeyCombination combo = m_seq[0];
    return Platform::registerHotKey(m_id, combo.key(), int(combo.keyboardModifiers()), &m_nativeRef);
#else
    return false;
#endif
}

void HotKey::unregisterNative() {
    if (!m_registered)
        return;
#ifdef Q_OS_WIN
    UnregisterHotKey(nullptr, m_id);
#elif defined(Q_OS_MACOS)
    Platform::unregisterHotKey(m_nativeRef);
    m_nativeRef = nullptr;
#endif
    m_registered = false;
}
