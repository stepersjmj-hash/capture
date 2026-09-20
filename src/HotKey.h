#pragma once

// 전역 단축키 — 앱이 트레이에 숨어 있어도 어디서나 동작한다.
// Windows: RegisterHotKey + 스레드 메시지(WM_HOTKEY)를 네이티브 이벤트 필터로 받는다.
// macOS: Carbon RegisterEventHotKey (Platform.mm). 접근성 권한 없이 동작한다.
#include <QKeySequence>
#include <QObject>

class HotKey : public QObject {
    Q_OBJECT
public:
    explicit HotKey(QObject *parent = nullptr);
    ~HotKey() override;

    // 등록 시도. 다른 앱이 이미 쓰는 조합이면 false (이전 등록은 해제된 상태).
    bool setSequence(const QKeySequence &seq);
    QKeySequence sequence() const { return m_seq; }
    bool isRegistered() const { return m_registered; }
    void clear();

signals:
    void activated();

private:
    bool registerNative();
    void unregisterNative();

    static int s_nextId;
    int m_id;
    QKeySequence m_seq;
    bool m_registered = false;
    void *m_nativeRef = nullptr;
    friend struct HotKeyDispatch;
};
