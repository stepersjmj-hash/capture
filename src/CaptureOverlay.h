#pragma once

// 영역 선택 오버레이 — 화면마다 하나씩 띄워 그 화면의 스크린샷 위에서 영역을 고른다.
// 드래그해서 놓으면 바로 확정(스니핑 도구와 같은 흐름), Enter/더블클릭 = 그 화면 전체, Esc/우클릭 = 취소.
#include <QImage>
#include <QList>
#include <QObject>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QWidget>

#include <functional>

class QScreen;

class CaptureOverlay : public QWidget {
    Q_OBJECT
public:
    CaptureOverlay(QScreen *screen, const QPixmap &shot);

signals:
    void selected(const QImage &image);
    void cancelled();

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;

private:
    QRect currentRect() const;
    void finish(const QRect &logical);
    void drawPill(QPainter &p, const QPointF &center, const QString &text) const;

    QScreen *m_screen;
    QPixmap m_shot;
    bool m_dragging = false;
    QPoint m_start, m_cur, m_mouse;
};

// 캡처 세션: 모든 화면에 오버레이를 띄우고, 하나가 결정되면 전부 닫는다.
class RegionCapture : public QObject {
    Q_OBJECT
public:
    // delayMs: 메뉴에서 호출할 때 메뉴가 닫힌 뒤 찍도록 잠깐 기다린다. 진행 중이면 무시.
    static void start(std::function<void(const QImage &)> onDone, int delayMs = 0);
    static bool active();
    // 마우스가 있는 화면 전체를 즉시 캡처
    static QImage grabFullScreen();

private:
    explicit RegionCapture(std::function<void(const QImage &)> onDone);
    void begin();
    void closeAll();

    std::function<void(const QImage &)> m_onDone;
    QList<CaptureOverlay *> m_overlays;
    static RegionCapture *s_active;
};
