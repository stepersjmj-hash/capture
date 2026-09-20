#include "ClipboardWatch.h"
#include "Platform.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

ClipboardWatch::ClipboardWatch(QObject *parent) : QObject(parent) {
    m_lastSeq = sequence();   // 시작 시점에 이미 들어 있던 이미지는 열지 않는다
    // Windows/Linux 는 dataChanged 가 즉시 오지만 macOS 는 외부 변경 알림이 없어 폴링이 필요하다.
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, [this] {
        ++m_fallbackSeq;
        poll();
    });
    connect(&m_timer, &QTimer::timeout, this, &ClipboardWatch::poll);
    m_timer.start(500);
    m_settle.setSingleShot(true);
    m_settle.setInterval(350);
    connect(&m_settle, &QTimer::timeout, this, &ClipboardWatch::settle);
}

void ClipboardWatch::setEnabled(bool on) {
    m_enabled = on;
    if (on)
        m_lastSeq = sequence();   // 켜기 전 내용은 무시
}

void ClipboardWatch::ignoreCurrent() {
    m_settle.stop();
    m_lastSeq = sequence();
}

quint64 ClipboardWatch::sequence() const {
#ifdef Q_OS_WIN
    return GetClipboardSequenceNumber();
#elif defined(Q_OS_MACOS)
    return Platform::pasteboardChangeCount();
#else
    return m_fallbackSeq;
#endif
}

// 변경 감지: 바로 읽지 않고 디바운스 타이머를 (재)시작한다
void ClipboardWatch::poll() {
    const quint64 seq = sequence();
    if (seq == m_lastSeq)
        return;
    m_lastSeq = seq;
    if (!m_enabled)
        return;
    m_retry = 0;
    m_settle.start();
}

void ClipboardWatch::settle() {
    m_lastSeq = sequence();
    const QMimeData *mime = QGuiApplication::clipboard()->mimeData();
    if (!mime || !mime->hasImage() || mime->hasHtml() || mime->hasText() || mime->hasUrls())
        return;
    const QImage img = QGuiApplication::clipboard()->image();
    if (img.isNull()) {
        // 다른 앱이 아직 클립보드를 쥐고 있을 수 있다 — 잠시 뒤 재시도(최대 3회)
        if (m_retry++ < 3)
            m_settle.start();
        return;
    }
    if (!m_lastImage.isNull() && img.size() == m_lastImage.size() && img == m_lastImage)
        return;
    m_lastImage = img;
    emit imageArrived(img);
}
