#pragma once

// 편집 캔버스 — 캡처 이미지를 창에 맞춰 보여 주고 그 위에 주석을 그린다.
// 항목은 이미지 픽셀 좌표로 들고 있으며, 그리기/히트 테스트는 배율(m_scale)로 변환한다.
#include "Annotation.h"

#include <QImage>
#include <QList>
#include <QPixmap>
#include <QWidget>

class QLineEdit;

class Canvas : public QWidget {
    Q_OBJECT
public:
    explicit Canvas(QWidget *parent = nullptr);

    void setImage(const QImage &img);
    const QImage &image() const { return m_img; }
    QImage flattened() const;            // 주석을 구운 결과 (저장·복사용)

    void setTool(Tool t);
    Tool tool() const { return m_tool; }
    void setColor(const QColor &c);      // 선택된 항목이 있으면 그 항목에도 적용
    QColor color() const { return m_color; }
    void setLineWidth(int w);
    int lineWidth() const { return m_width; }
    void setTextPx(int px);
    int textPx() const { return m_textPx; }

    void undo();
    void redo();
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    void deleteSelected();
    bool hasSelection() const { return m_sel >= 0; }
    bool hasItems() const { return !m_items.isEmpty(); }
    int revision() const { return m_rev; }

    void finishTextEdit();               // 입력 중인 텍스트를 확정 (저장·복사 전에 호출)
    bool cancelPending();                // ESC 단계별: 텍스트 입력 취소 → 그리기 취소 → 선택 해제. 할 게 없으면 false

    QSize sizeHint() const override;

signals:
    void changed();
    void selectionChanged();
    void hint(const QString &text);

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;

private:
    void relayout();
    QPointF toImage(const QPointF &w) const { return (w - m_origin) / m_scale; }
    QPointF toWidget(const QPointF &i) const { return m_origin + i * m_scale; }
    int hitTest(const QPointF &img) const;
    void pushUndo();
    void bump();
    void select(int idx);
    void beginTextEdit(const QPointF &imgPos, int editIndex);
    void commitTextEdit();
    void placeTextEdit();
    void snapLine(QPointF &p2, const QPointF &p1, bool free) const;
    void updateCursor(const QPointF &imgPos);
    QString toolHint() const;

    QImage m_img;
    QPixmap m_scaled;                    // 현재 배율로 미리 축소한 사본 (마우스 이동마다 재축소 방지)
    QList<Item> m_items;
    QList<QList<Item>> m_undo, m_redo;
    Tool m_tool = Tool::Rect;
    QColor m_color = QColor(0xff, 0x3b, 0x30);
    int m_width = 3;
    int m_textPx = 28;
    int m_rev = 0;

    qreal m_scale = 1.0;
    QPointF m_origin;

    bool m_drawing = false;
    Item m_cur;
    int m_sel = -1;
    bool m_moving = false;
    bool m_movePushed = false;
    QPointF m_lastPos;

    QLineEdit *m_edit = nullptr;
    int m_editIndex = -1;                // 기존 텍스트를 고치는 중이면 그 인덱스
    QPointF m_editPos;                   // 새 텍스트 좌상단 (이미지 좌표)
    bool m_committing = false;
};
