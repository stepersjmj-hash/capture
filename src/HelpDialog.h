#pragma once

// 단축키 치트시트 (F1) — Mview 의 단축키 오버레이(디자인 09)를 Qt 로 옮긴 것.
// 부모 창을 덮는 딤 + 유리 패널, 2열 목록(그룹 제목 + 라벨 + 키 배지). 아무 곳이나 클릭하거나
// Esc·F1 로 닫는다. 패널이 부모 창보다 크면 창 밖으로 넓혀서 덮는다 (작은 캡처 창 대비).
#include <QDialog>
#include <QList>
#include <QSizeF>
#include <QString>

class HelpDialog : public QDialog {
    Q_OBJECT
public:
    struct Item {
        QString label;
        QString keys;
    };
    struct Group {
        QString title;
        QList<Item> items;
    };

    HelpDialog(QWidget *parent, const QList<Group> &colA, const QList<Group> &colB);

protected:
    void showEvent(QShowEvent *) override;
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;

private:
    qreal columnHeight(const QList<Group> &col) const;
    QSizeF panelSize() const;
    void drawColumn(QPainter &p, const QList<Group> &col, qreal x, qreal y, qreal colW) const;
    void drawKeyBadge(QPainter &p, qreal rightX, qreal cy, const QString &text) const;
    void drawKeyboardGlyph(QPainter &p, const QRectF &r) const;

    QList<Group> m_colA, m_colB;
};
