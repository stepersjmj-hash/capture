#pragma once

// 클립보드 감시 — OS 기본 캡처(Windows Win+Shift+S, macOS Ctrl+Shift+Cmd+4 등)는 결과를
// 클립보드에 넣는다. 새 이미지가 들어오면 편집 창을 띄운다. 텍스트/HTML/URL 이 함께 실린
// 경우(브라우저 "이미지 복사" 등)는 캡처가 아니므로 무시한다.
// 앱들은 포맷을 여러 번 나눠 넣어 시퀀스가 연달아 바뀌므로, 잠깐 잠잠해진 뒤 한 번만 읽는다.
#include <QImage>
#include <QObject>
#include <QTimer>

class ClipboardWatch : public QObject {
    Q_OBJECT
public:
    explicit ClipboardWatch(QObject *parent = nullptr);

    void setEnabled(bool on);
    bool enabled() const { return m_enabled; }
    // 우리가 방금 넣은 내용(복사·자동 복사)은 감지하지 않도록 현재 상태를 기준점으로 삼는다
    void ignoreCurrent();

signals:
    void imageArrived(const QImage &image);

private:
    void poll();
    void settle();
    quint64 sequence() const;

    QTimer m_timer;
    QTimer m_settle;             // 변경 감지 후 잠잠해질 때까지 기다리는 디바운스
    bool m_enabled = true;
    quint64 m_lastSeq = 0;
    quint64 m_fallbackSeq = 0;   // 시퀀스 API 가 없는 플랫폼: dataChanged 횟수
    QImage m_lastImage;          // 같은 이미지가 연달아 오면 한 번만 연다
    int m_retry = 0;
};
