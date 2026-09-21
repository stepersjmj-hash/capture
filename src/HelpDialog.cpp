#include "HelpDialog.h"
#include "Theme.h"

#include <QFontMetricsF>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>

namespace {
// Mview 오버레이와 같은 치수 (논리 픽셀, 96dpi 기준)
constexpr qreal kPad = 26, kHeaderH = 24, kHeadGap = 20, kColGap = 34;
constexpr qreal kTitleH = 26, kRowH = 30, kGroupGap = 18;
constexpr qreal kPanelW = 760, kBadgeH = 22;

QFont sized(int px, bool bold = false) {
    QFont f;
    f.setFamilies({"Pretendard", "Malgun Gothic", "Apple SD Gothic Neo", "Segoe UI", "sans-serif"});
    f.setPixelSize(px);
    f.setBold(bold);
    return f;
}
} // namespace

HelpDialog::HelpDialog(QWidget *parent, const QList<Group> &colA, const QList<Group> &colB)
    : QDialog(parent), m_colA(colA), m_colB(colB) {
    setObjectName("helpOverlay");
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setStyleSheet("QDialog#helpOverlay { background: transparent; }");
    setModal(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::ArrowCursor);

    // 부모 창을 덮는다. 패널이 창보다 크면(작은 캡처 → 작은 창) 필요한 만큼 넓혀서 화면 안에 둔다.
    QWidget *win = parent ? parent->window() : nullptr;
    QRect g = win ? win->frameGeometry() : QRect(0, 0, 900, 700);
    const QSizeF need = panelSize() + QSizeF(40, 40);
    if (g.width() < need.width() || g.height() < need.height()) {
        QRect grown(QPoint(0, 0), QSizeF(qMax(qreal(g.width()), need.width()),
                                         qMax(qreal(g.height()), need.height())).toSize());
        grown.moveCenter(g.center());
        QScreen *sc = win ? win->screen() : QGuiApplication::primaryScreen();
        if (sc) {
            const QRect avail = sc->availableGeometry();
            grown.setSize(grown.size().boundedTo(avail.size()));
            if (grown.right() > avail.right())
                grown.moveRight(avail.right());
            if (grown.bottom() > avail.bottom())
                grown.moveBottom(avail.bottom());
            if (grown.left() < avail.left())
                grown.moveLeft(avail.left());
            if (grown.top() < avail.top())
                grown.moveTop(avail.top());
        }
        g = grown;
    }
    setGeometry(g);
}

// 프레임 없는 반투명 창이라 그냥 두면 활성 창이 안 되어 Esc 가 안 먹는다 — 직접 올려서 포커스를 준다
void HelpDialog::showEvent(QShowEvent *e) {
    QDialog::showEvent(e);
    raise();
    activateWindow();
    setFocus(Qt::OtherFocusReason);
}

qreal HelpDialog::columnHeight(const QList<Group> &col) const {
    qreal h = 0;
    for (int i = 0; i < col.size(); ++i) {
        h += kTitleH + kRowH * col[i].items.size();
        if (i + 1 < col.size())
            h += kGroupGap;
    }
    return h;
}

QSizeF HelpDialog::panelSize() const {
    const qreal body = qMax(columnHeight(m_colA), columnHeight(m_colB));
    return QSizeF(kPanelW, kPad + kHeaderH + kHeadGap + body + kPad);
}

void HelpDialog::drawKeyBadge(QPainter &p, qreal rightX, qreal cy, const QString &text) const {
    const QFont f = sized(11, true);
    const QFontMetricsF fm(f);
    const qreal w = fm.horizontalAdvance(text) + 16;
    const QRectF r(rightX - w, cy - kBadgeH / 2, w, kBadgeH);
    QPainterPath path;
    path.addRoundedRect(r, 6, 6);
    p.fillPath(path, QColor(255, 255, 255, 18));
    p.setPen(QPen(QColor(255, 255, 255, 26), 1));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
    p.setFont(f);
    p.setPen(Theme::text1());
    p.drawText(r, Qt::AlignCenter, text);
}

void HelpDialog::drawColumn(QPainter &p, const QList<Group> &col, qreal x, qreal y, qreal colW) const {
    const QFont titleF = sized(12, true);
    const QFont labelF = sized(13);
    qreal cy = y;
    for (int gi = 0; gi < col.size(); ++gi) {
        p.setFont(titleF);
        p.setPen(Theme::accent());
        p.drawText(QRectF(x, cy, colW, kTitleH), Qt::AlignVCenter | Qt::AlignLeft, col[gi].title);
        cy += kTitleH;
        for (const Item &it : col[gi].items) {
            p.setFont(labelF);
            p.setPen(QColor(0xc7, 0xca, 0xd2));
            p.drawText(QRectF(x, cy, colW * 0.58, kRowH), Qt::AlignVCenter | Qt::AlignLeft, it.label);
            drawKeyBadge(p, x + colW, cy + kRowH / 2, it.keys);
            p.setPen(QPen(QColor(255, 255, 255, 12), 1));
            p.drawLine(QPointF(x, cy + kRowH - 1), QPointF(x + colW, cy + kRowH - 1));
            cy += kRowH;
        }
        if (gi + 1 < col.size())
            cy += kGroupGap;
    }
}

// 헤더의 작은 키보드 글리프 (Mview 와 같은 모양 — 폰트 없이 선으로)
void HelpDialog::drawKeyboardGlyph(QPainter &p, const QRectF &r) const {
    QPen pen(Theme::accent(), 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r, 3, 3);
    for (int row = 0; row < 2; ++row)
        for (int col = 0; col < 4; ++col) {
            const qreal dx = r.x() + 3.5 + col * 4.0, dy = r.y() + 3.5 + row * 4.0;
            p.drawLine(QPointF(dx, dy), QPointF(dx + 0.5, dy));
        }
    p.drawLine(QPointF(r.x() + 5, r.bottom() - 3.5), QPointF(r.right() - 5, r.bottom() - 3.5));
}

void HelpDialog::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    const QRectF full(rect());
    p.fillRect(full, QColor(8, 8, 11, 189));

    const QSizeF ps = panelSize();
    const qreal panelW = qMin(ps.width(), qMax(320.0, full.width() - 60));
    const qreal panelH = ps.height();
    QRectF panel((full.width() - panelW) / 2, qMax(20.0, (full.height() - panelH) / 2), panelW, panelH);

    QPainterPath pp;
    pp.addRoundedRect(panel, 16, 16);
    p.fillPath(pp, QColor(24, 24, 30, 235));
    p.setPen(QPen(QColor(255, 255, 255, 26), 1));
    p.setBrush(Qt::NoBrush);
    p.drawPath(pp);
    p.setPen(QPen(QColor(255, 255, 255, 18), 1));
    p.drawLine(QPointF(panel.left() + 16, panel.top() + 1), QPointF(panel.right() - 16, panel.top() + 1));

    // 헤더: 키보드 글리프 + "단축키" + 오른쪽 "닫기 [ESC]"
    const qreal hx = panel.left() + kPad, hy = panel.top() + kPad;
    drawKeyboardGlyph(p, QRectF(hx, hy + (kHeaderH - 14) / 2, 20, 14));
    p.setFont(sized(15, true));
    p.setPen(Theme::text1());
    p.drawText(QRectF(hx + 30, hy, 240, kHeaderH), Qt::AlignVCenter | Qt::AlignLeft, "단축키");
    {
        const qreal rx = panel.right() - kPad;
        drawKeyBadge(p, rx, hy + kHeaderH / 2, "ESC");
        const QFont f = sized(12);
        const QFontMetricsF fm(f);
        const qreal escW = fm.horizontalAdvance("ESC") + 16;
        p.setFont(f);
        p.setPen(Theme::text2());
        p.drawText(QRectF(rx - escW - 70, hy, 60, kHeaderH), Qt::AlignVCenter | Qt::AlignRight, "닫기");
    }

    const qreal colW = (panelW - kPad * 2 - kColGap) / 2;
    const qreal by = hy + kHeaderH + kHeadGap;
    drawColumn(p, m_colA, panel.left() + kPad, by, colW);
    drawColumn(p, m_colB, panel.left() + kPad + colW + kColGap, by, colW);
}

void HelpDialog::mousePressEvent(QMouseEvent *) { accept(); }

void HelpDialog::keyPressEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_F1 || e->key() == Qt::Key_Escape || e->key() == Qt::Key_Return ||
        e->key() == Qt::Key_Enter || e->key() == Qt::Key_Space) {
        accept();
        return;
    }
    QDialog::keyPressEvent(e);
}
