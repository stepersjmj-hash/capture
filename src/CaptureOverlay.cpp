#include "CaptureOverlay.h"
#include "Platform.h"
#include "Theme.h"

#include <QApplication>
#include <QCursor>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QTimer>
#include <QWindow>

// ── 오버레이 ─────────────────────────────────────────────
CaptureOverlay::CaptureOverlay(QScreen *screen, const QPixmap &shot)
    : QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool |
                           Qt::NoDropShadowWindowHint),
      m_screen(screen), m_shot(shot) {
    setAttribute(Qt::WA_DeleteOnClose);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);
    setFocusPolicy(Qt::StrongFocus);
    setScreen(screen);
    setGeometry(screen->geometry());
    m_mouse = mapFromGlobal(QCursor::pos());
}

QRect CaptureOverlay::currentRect() const {
    if (!m_dragging)
        return QRect();
    // 끌어간 거리 그대로의 크기 (QRect(p1, p2) 는 양 끝을 포함해 1px 커진다)
    return QRect(qMin(m_start.x(), m_cur.x()), qMin(m_start.y(), m_cur.y()), qAbs(m_cur.x() - m_start.x()),
                 qAbs(m_cur.y() - m_start.y()));
}

void CaptureOverlay::finish(const QRect &logical) {
    if (m_done)
        return;
    m_done = true;
    // 논리 좌표 → 실제 픽셀 (고DPI 화면은 배율만큼 크다)
    const qreal dpr = m_shot.devicePixelRatio();
    QImage img = m_shot.toImage();
    img.setDevicePixelRatio(1.0);
    const QRect px(qRound(logical.x() * dpr), qRound(logical.y() * dpr), qRound(logical.width() * dpr),
                   qRound(logical.height() * dpr));
    const QRect clipped = px.intersected(img.rect());
    if (clipped.width() < 1 || clipped.height() < 1) {
        emit cancelled();
        return;
    }
    emit selected(img.copy(clipped));
}

void CaptureOverlay::mousePressEvent(QMouseEvent *e) {
    if (m_done)
        return;
    if (e->button() == Qt::RightButton) {
        emit cancelled();
        return;
    }
    if (e->button() != Qt::LeftButton)
        return;
    m_dragging = true;
    m_start = m_cur = e->pos();
    update();
}

void CaptureOverlay::mouseMoveEvent(QMouseEvent *e) {
    m_mouse = e->pos();
    if (m_dragging)
        m_cur = e->pos();
    update();
}

void CaptureOverlay::mouseReleaseEvent(QMouseEvent *e) {
    if (e->button() != Qt::LeftButton || !m_dragging)
        return;
    m_cur = e->pos();
    const QRect r = currentRect();
    m_dragging = false;
    if (r.width() >= 3 && r.height() >= 3)
        finish(r);
    else
        update();   // 클릭만 한 경우: 선택 없음 상태로
}

void CaptureOverlay::keyPressEvent(QKeyEvent *e) {
    if (m_done)
        return;
    switch (e->key()) {
    case Qt::Key_Escape:
        emit cancelled();
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Space:
        finish(rect());
        break;
    default:
        QWidget::keyPressEvent(e);
    }
}

void CaptureOverlay::drawPill(QPainter &p, const QPointF &center, const QString &text) const {
    QFont f = font();
    f.setPixelSize(13);
    f.setWeight(QFont::DemiBold);
    p.setFont(f);
    const QFontMetricsF fm(f);
    const qreal w = fm.horizontalAdvance(text) + 28, h = 30;
    const QRectF r(center.x() - w / 2, center.y() - h / 2, w, h);
    QPainterPath path;
    path.addRoundedRect(r, h / 2, h / 2);
    QColor border = Theme::accent();
    border.setAlpha(90);
    p.setPen(QPen(border, 1));
    p.setBrush(QColor(18, 18, 22, 225));
    p.drawPath(path);
    p.setPen(Theme::text1());
    p.drawText(r, Qt::AlignCenter, text);
}

void CaptureOverlay::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.drawPixmap(rect(), m_shot);
    p.fillRect(rect(), QColor(0, 0, 0, 115));

    const QRect sel = currentRect();
    if (sel.width() > 0 && sel.height() > 0) {
        p.save();
        p.setClipRect(sel);
        p.drawPixmap(rect(), m_shot);
        p.restore();
        p.setPen(QPen(Theme::accent(), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(sel).adjusted(-1, -1, 0, 0));
        // 크기 라벨 — 선택 영역 위, 자리가 없으면 아래
        const QString label = QString("%1 × %2").arg(sel.width()).arg(sel.height());
        const qreal ly = sel.top() >= 40 ? sel.top() - 22 : sel.bottom() + 24;
        drawPill(p, QPointF(sel.center().x(), ly), label);
    } else {
        p.setPen(QPen(QColor(255, 255, 255, 80), 1));
        p.drawLine(0, m_mouse.y(), width(), m_mouse.y());
        p.drawLine(m_mouse.x(), 0, m_mouse.x(), height());
        drawPill(p, QPointF(width() / 2.0, 40),
                 QStringLiteral("드래그로 영역 선택   ·   Enter 이 화면 전체   ·   Esc 취소"));
    }
}

// ── 세션 ─────────────────────────────────────────────────
RegionCapture *RegionCapture::s_active = nullptr;

bool RegionCapture::active() { return s_active != nullptr; }

RegionCapture::RegionCapture(std::function<void(const QImage &)> onDone)
    : QObject(nullptr), m_onDone(std::move(onDone)) {}

void RegionCapture::start(std::function<void(const QImage &)> onDone, int delayMs) {
    if (s_active)
        return;
    s_active = new RegionCapture(std::move(onDone));
    if (delayMs > 0)
        QTimer::singleShot(delayMs, s_active, &RegionCapture::begin);
    else
        s_active->begin();
}

QImage RegionCapture::grabFullScreen() {
    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return QImage();
    QImage img = screen->grabWindow(0).toImage();
    img.setDevicePixelRatio(1.0);
    return img;
}

void RegionCapture::begin() {
    const QPoint cursor = QCursor::pos();
    CaptureOverlay *focusTarget = nullptr;
    // 먼저 모든 화면을 찍은 뒤에 창을 띄운다 (오버레이가 다른 화면 스크린샷에 찍히지 않게)
    QList<QPair<QScreen *, QPixmap>> shots;
    for (QScreen *s : QGuiApplication::screens())
        shots.append({s, s->grabWindow(0)});
    for (const auto &pair : shots) {
        QScreen *screen = pair.first;
        auto *ov = new CaptureOverlay(screen, pair.second);
        connect(ov, &CaptureOverlay::selected, this, [this](const QImage &img) {
            auto done = std::move(m_onDone);
            closeAll();
            if (done)
                QTimer::singleShot(0, qApp, [done, img] { done(img); });
        });
        connect(ov, &CaptureOverlay::cancelled, this, [this] { closeAll(); });
        m_overlays.append(ov);
        ov->show();
#ifdef Q_OS_MACOS
        Platform::raiseOverlay(ov);
#endif
        if (screen->geometry().contains(cursor))
            focusTarget = ov;
    }
    if (!focusTarget && !m_overlays.isEmpty())
        focusTarget = m_overlays.first();
    if (focusTarget) {
        focusTarget->raise();
        focusTarget->activateWindow();
        focusTarget->setFocus();
    }
}

void RegionCapture::closeAll() {
    for (CaptureOverlay *ov : m_overlays) {
        ov->hide();
        ov->deleteLater();
    }
    m_overlays.clear();
    s_active = nullptr;
    deleteLater();
}
