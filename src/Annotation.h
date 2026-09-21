#pragma once

// 주석(편집) 항목 — 사각형 · 선(밑줄) · 화살표 · 텍스트 · 채우기.
// 좌표·크기는 모두 원본 이미지 픽셀 기준이라 화면 배율과 무관하게 저장/굽기가 같다.
#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QList>
#include <QLineF>
#include <QPainter>
#include <QPainterPath>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QtMath>

// Region 은 화면 위 선택 영역(마퀴)만 다루는 도구라 Item 으로 저장되지 않는다.
enum class Tool { Select, Region, Rect, Line, Arrow, Text, Fill };

struct Item {
    Tool type = Tool::Rect;
    QPointF p1, p2;      // Rect/Fill: 대각 꼭짓점 · Line/Arrow: 시작→끝 · Text: p1 = 좌상단
    QColor color = QColor(0xff, 0x3b, 0x30);
    int width = 3;       // 선 굵기 (이미지 픽셀)
    QString text;
    int textPx = 28;     // 글자 크기 (이미지 픽셀)
    bool outline = false;   // 텍스트 어두운 외곽선 (기본 없음 — 우클릭으로 켠다)

    QRectF rect() const { return QRectF(p1, p2).normalized(); }
    void move(const QPointF &d) { p1 += d; p2 += d; }
};

namespace Annot {

inline QFont textFont(int px) {
    QFont f;
    f.setFamilies({"Pretendard", "Malgun Gothic", "Apple SD Gothic Neo", "Segoe UI", "sans-serif"});
    f.setPixelSize(qMax(4, px));
    f.setBold(true);
    return f;
}

inline qreal textPad(const Item &it) { return qMax(2.0, it.textPx / 10.0); }

// 텍스트 항목의 차지 영역 (좌상단 p1 기준)
inline QRectF textRect(const Item &it) {
    const QFontMetricsF fm(textFont(it.textPx));
    const qreal pad = textPad(it);
    return QRectF(it.p1.x(), it.p1.y(), fm.horizontalAdvance(it.text) + pad * 2, fm.height() + pad * 2);
}

inline QRectF bounds(const Item &it) {
    switch (it.type) {
    case Tool::Text: return textRect(it);
    case Tool::Line:
    case Tool::Arrow: return QRectF(it.p1, it.p2).normalized().adjusted(-it.width, -it.width, it.width, it.width);
    default: return it.rect();
    }
}

inline void paint(QPainter &p, const Item &it) {
    switch (it.type) {
    case Tool::Rect: {
        QPen pen(it.color, it.width, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRect(it.rect());
        break;
    }
    case Tool::Fill:
        p.setPen(Qt::NoPen);
        p.setBrush(it.color);
        p.drawRect(it.rect());
        break;
    case Tool::Line: {
        QPen pen(it.color, it.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.drawLine(it.p1, it.p2);
        break;
    }
    case Tool::Arrow: {
        const QLineF line(it.p1, it.p2);
        if (line.length() < 0.5)
            break;
        const qreal head = qMax(12.0, it.width * 4.5);
        const QPointF dir = (it.p2 - it.p1) / line.length();   // 화면 좌표 방향 벡터
        const QPointF tip = it.p2;
        const QPointF base = tip - dir * head;
        const QPointF nrm(-dir.y(), dir.x());
        const qreal half = head * 0.55;
        QPolygonF tri;
        tri << tip << (base + nrm * half) << (base - nrm * half);
        QPen pen(it.color, it.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.drawLine(it.p1, base + dir * (head * 0.25));   // 머리 안쪽까지만 그려 겹침 방지
        p.setPen(Qt::NoPen);
        p.setBrush(it.color);
        p.drawPolygon(tri);
        break;
    }
    case Tool::Text: {
        if (it.text.isEmpty())
            break;
        const QFont f = textFont(it.textPx);
        const QFontMetricsF fm(f);
        const qreal pad = textPad(it);
        QPainterPath path;
        path.addText(it.p1.x() + pad, it.p1.y() + pad + fm.ascent(), f, it.text);
        // 외곽선은 선택 사항 — 켜면 밝은 배경에서도 읽힌다 (우클릭 "테두리 추가")
        if (it.outline) {
            QPen outline(QColor(0, 0, 0, 200), qMax(1.0, it.textPx / 12.0), Qt::SolidLine, Qt::RoundCap,
                         Qt::RoundJoin);
            p.setPen(outline);
            p.setBrush(Qt::NoBrush);
            p.drawPath(path);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(it.color);
        p.drawPath(path);
        break;
    }
    case Tool::Select:
    case Tool::Region:
        break;
    }
}

inline void paintAll(QPainter &p, const QList<Item> &items) {
    for (const Item &it : items)
        paint(p, it);
}

inline qreal distToSegment(const QPointF &pt, const QPointF &a, const QPointF &b) {
    const QPointF ab = b - a;
    const qreal len2 = ab.x() * ab.x() + ab.y() * ab.y();
    qreal t = len2 > 0 ? ((pt.x() - a.x()) * ab.x() + (pt.y() - a.y()) * ab.y()) / len2 : 0;
    t = qBound(0.0, t, 1.0);
    const QPointF proj = a + ab * t;
    return QLineF(pt, proj).length();
}

// 히트 테스트 (tol = 허용 오차, 이미지 픽셀)
inline bool hit(const Item &it, const QPointF &pt, qreal tol) {
    switch (it.type) {
    case Tool::Rect: {
        const QRectF r = it.rect();
        const qreal half = it.width / 2.0 + tol;
        const bool outer = r.adjusted(-half, -half, half, half).contains(pt);
        const bool inner = r.adjusted(half, half, -half, -half).contains(pt);
        return outer && !inner;
    }
    case Tool::Fill: return it.rect().adjusted(-tol, -tol, tol, tol).contains(pt);
    case Tool::Line:
    case Tool::Arrow: return distToSegment(pt, it.p1, it.p2) <= it.width / 2.0 + tol;
    case Tool::Text: return textRect(it).adjusted(-tol, -tol, tol, tol).contains(pt);
    case Tool::Select:
    case Tool::Region: return false;
    }
    return false;
}

} // namespace Annot
