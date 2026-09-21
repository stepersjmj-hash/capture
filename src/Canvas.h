#pragma once

// 편집 캔버스 — 캡처 이미지를 창에 맞춰 보여 주고 그 위에 주석을 그린다.
// 항목은 이미지 픽셀 좌표로 들고 있으며, 그리기/히트 테스트는 배율(m_scale)로 변환한다.
#include "Annotation.h"

#include <QImage>
#include <QList>
#include <QPixmap>
#include <QRectF>
#include <QWidget>

class QLineEdit;
class QTimer;

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
    void setTextOutline(bool on);        // 선택된 텍스트 + 앞으로 넣을 텍스트의 외곽선

    // 선택 영역(마퀴) — Region 도구. 우클릭 메뉴/Enter 로 자르기·채우기·테두리.
    bool hasRegion() const { return m_hasRegion; }
    void selectAllRegion();              // 이미지 전체를 선택 영역으로 (Ctrl+A)
    void cropToRegion();
    void fillRegion();
    void borderRegion();
    void clearRegion();

    void undo();
    void redo();
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    void deleteSelected();
    bool hasSelection() const { return m_sel >= 0; }
    bool hasItems() const { return !m_items.isEmpty(); }
    bool hasContent() const { return !m_items.isEmpty() || m_imageEdited; }   // 저장할 거리가 있나 (자르기 포함)
    int revision() const { return m_rev; }

    void finishTextEdit();               // 입력 중인 텍스트를 확정 (저장·복사 전에 호출)
    bool cancelPending();                // ESC 단계별: 텍스트 입력 취소 → 그리기 취소 → 선택 해제. 할 게 없으면 false

    QSize sizeHint() const override;

signals:
    void changed();
    void selectionChanged();
    void hint(const QString &text);
    // 선택·휠·손잡이로 값이 바뀌었을 때 툴바 스핀/색 칩을 맞추기 위한 알림
    void textPxChanged(int px);
    void lineWidthChanged(int w);
    void colorChanged(const QColor &c);
    void imageResized(const QSize &size);   // 자르기/되돌리기로 이미지 크기가 바뀜
    void colorPickRequested();              // 우클릭 "색 변경…" — 창이 색 대화상자를 연다
    void menuRequested(const QPoint &globalPos);   // 빈 곳 우클릭 — 도구·도움말 메뉴 (편집 창이 띄운다)

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void contextMenuEvent(QContextMenuEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void wheelEvent(QWheelEvent *) override;

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
    QRectF handleRect() const;           // 선택된 텍스트의 크기 조절 손잡이 (위젯 좌표)
    void applyTextPx(int px);
    QString toolHint() const;
    QRectF regionF() const;              // 정규화된 선택 영역 (이미지 좌표)
    QPointF clampToImage(const QPointF &p) const;
    void squareRegion();                 // Shift: 정사각형 보정 (Mview 자르기와 같은 규칙)
    void syncMarquee();                  // 행진 점선 타이머 켜기/끄기
    void paintRegion(QPainter &p, const QRectF &dst);
    void showRegionMenu(const QPoint &globalPos);
    void showItemMenu(int idx, const QPoint &globalPos);

    // 되돌리기 한 칸 — 이미지도 함께 (자르기). QImage 는 암묵적 공유라 안 바뀐 단계는 사본을 안 만든다.
    struct Snapshot {
        QList<Item> items;
        QImage img;
        bool imageEdited = false;
    };

    void restore(const Snapshot &s);     // 되돌리기/다시 실행 공통

    QImage m_img;
    QPixmap m_scaled;                    // 현재 배율로 미리 축소한 사본 (마우스 이동마다 재축소 방지)
    QList<Item> m_items;
    QList<Snapshot> m_undo, m_redo;
    Tool m_tool = Tool::Region;
    QColor m_color = QColor(0xff, 0x3b, 0x30);
    int m_width = 3;
    int m_textPx = 28;
    bool m_textOutline = false;          // 새 텍스트의 외곽선 기본값 (우클릭으로 바꾸면 이어짐)
    int m_rev = 0;
    bool m_imageEdited = false;          // 자르기로 이미지 자체가 바뀌었나

    qreal m_scale = 1.0;
    QPointF m_origin;

    bool m_drawing = false;
    Item m_cur;
    int m_sel = -1;
    bool m_moving = false;
    bool m_movePushed = false;
    QPointF m_lastPos;
    bool m_resizing = false;             // 텍스트 손잡이 드래그 중
    qreal m_rsStartW = 0;
    int m_rsStartPx = 0;

    // 선택 영역
    bool m_hasRegion = false;
    bool m_regionDrag = false;
    bool m_regionMove = false;
    QPointF m_rgA, m_rgB;                // 앵커 · 현재점 (이미지 좌표)
    QPointF m_regionGrab;                // 잡은 지점 − 영역 좌상단
    qreal m_dashPhase = 0;
    QTimer *m_marquee = nullptr;

    QLineEdit *m_edit = nullptr;
    int m_editIndex = -1;                // 기존 텍스트를 고치는 중이면 그 인덱스
    QPointF m_editPos;                   // 새 텍스트 좌상단 (이미지 좌표)
    bool m_committing = false;
};
