#include "Canvas.h"
#include "Theme.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QWheelEvent>
#include <QtMath>
#include <cmath>

namespace {
constexpr int kMargin = 12;
constexpr qreal kHitTolPx = 6.0;   // 화면 픽셀 기준 허용 오차
constexpr int kMinTextPx = 6, kMaxTextPx = 400;
constexpr qreal kHandle = 10.0;    // 텍스트 크기 조절 손잡이 한 변 (화면 픽셀)
} // namespace

Canvas::Canvas(QWidget *parent) : QWidget(parent) {
    setObjectName("canvas");
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::CrossCursor);

    // 선택 영역 점선 행진 (Mview 자르기와 같은 60ms 주기)
    m_marquee = new QTimer(this);
    m_marquee->setInterval(60);
    connect(m_marquee, &QTimer::timeout, this, [this] {
        m_dashPhase += 1.6;
        update();
    });

    m_edit = new QLineEdit(this);
    m_edit->hide();
    m_edit->setFrame(false);
    m_edit->setAttribute(Qt::WA_MacShowFocusRect, false);
    connect(m_edit, &QLineEdit::returnPressed, this, &Canvas::commitTextEdit);
    connect(m_edit, &QLineEdit::editingFinished, this, &Canvas::commitTextEdit);
    connect(m_edit, &QLineEdit::textChanged, this, &Canvas::placeTextEdit);
}

QSize Canvas::sizeHint() const {
    if (m_img.isNull())
        return QSize(640, 400);
    return m_img.size() + QSize(kMargin * 2, kMargin * 2);
}

void Canvas::setImage(const QImage &img) {
    m_img = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    m_img.setDevicePixelRatio(1.0);
    m_items.clear();
    m_undo.clear();
    m_redo.clear();
    m_sel = -1;
    m_hasRegion = m_regionDrag = m_regionMove = false;
    m_imageEdited = false;
    syncMarquee();
    m_scaled = QPixmap();
    relayout();
    update();
}

void Canvas::relayout() {
    if (m_img.isNull())
        return;
    const QSizeF avail(width() - kMargin * 2, height() - kMargin * 2);
    qreal s = qMin(avail.width() / m_img.width(), avail.height() / m_img.height());
    s = qMin(1.0, qMax(0.01, s));
    const bool scaleChanged = !qFuzzyCompare(s, m_scale) || m_scaled.isNull();
    m_scale = s;
    const QSizeF shown = QSizeF(m_img.size()) * m_scale;
    m_origin = QPointF((width() - shown.width()) / 2.0, (height() - shown.height()) / 2.0);
    if (scaleChanged) {
        const qreal dpr = devicePixelRatioF();
        const QSize px = (shown * dpr).toSize();
        if (px.isEmpty()) {
            m_scaled = QPixmap();
        } else {
            m_scaled = QPixmap::fromImage(
                m_scale >= 1.0 ? m_img : m_img.scaled(px, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
            m_scaled.setDevicePixelRatio(dpr);
        }
    }
    placeTextEdit();
}

void Canvas::resizeEvent(QResizeEvent *) { relayout(); }

QImage Canvas::flattened() const {
    QImage out = m_img.copy();
    if (!m_items.isEmpty()) {
        QPainter p(&out);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::TextAntialiasing);
        Annot::paintAll(p, m_items);
    }
    return out.convertToFormat(QImage::Format_ARGB32);
}

// ── 도구·속성 ─────────────────────────────────────────────
void Canvas::setTool(Tool t) {
    if (m_edit->isVisible())
        commitTextEdit();
    m_tool = t;
    m_drawing = false;
    if (t != Tool::Region)
        clearRegion();
    if (t != Tool::Select && t != Tool::Text)
        select(-1);
    updateCursor(toImage(mapFromGlobal(QCursor::pos())));
    emit hint(toolHint());
    update();
}

void Canvas::setColor(const QColor &c) {
    m_color = c;
    if (m_sel >= 0 && m_items[m_sel].color != c) {
        pushUndo();
        m_items[m_sel].color = c;
        bump();
    }
    if (m_edit->isVisible())
        placeTextEdit();
}

void Canvas::setLineWidth(int w) {
    m_width = w;
    if (m_sel >= 0 && m_items[m_sel].type != Tool::Text && m_items[m_sel].type != Tool::Fill &&
        m_items[m_sel].width != w) {
        pushUndo();
        m_items[m_sel].width = w;
        bump();
    }
}

void Canvas::setTextPx(int px) {
    px = qBound(kMinTextPx, px, kMaxTextPx);
    m_textPx = px;
    if (m_sel >= 0 && m_items[m_sel].type == Tool::Text && m_items[m_sel].textPx != px) {
        pushUndo();
        m_items[m_sel].textPx = px;
        bump();
    }
    if (m_edit->isVisible())
        placeTextEdit();
}

// 우클릭 "테두리" — 선택된 텍스트에 적용하고 앞으로 넣을 텍스트의 기본값도 바꾼다
void Canvas::setTextOutline(bool on) {
    m_textOutline = on;
    if (m_sel >= 0 && m_items[m_sel].type == Tool::Text && m_items[m_sel].outline != on) {
        pushUndo();
        m_items[m_sel].outline = on;
        bump();
    }
}

// 휠·손잡이에서 온 크기 변경: 항목에 적용하고 툴바 스핀에도 알린다
void Canvas::applyTextPx(int px) {
    px = qBound(kMinTextPx, px, kMaxTextPx);
    if (px == m_textPx)
        return;
    setTextPx(px);
    emit textPxChanged(px);
}

QString Canvas::toolHint() const {
    switch (m_tool) {
    case Tool::Select: return QStringLiteral("클릭으로 선택 · 드래그로 이동 · 휠/글자 칸/오른쪽 아래 손잡이로 크기 · 우클릭 메뉴 · Delete 삭제 · 텍스트는 더블클릭으로 수정");
    case Tool::Region: return QStringLiteral("드래그로 영역 선택 (Shift 정사각형) · 안쪽 드래그 = 이동 · 우클릭 = 자르기·채우기·테두리 · Enter 자르기 · Esc 선택 지우기");
    case Tool::Rect: return QStringLiteral("드래그로 사각형 표시");
    case Tool::Line: return QStringLiteral("드래그로 밑줄(선) — 수평·수직에 가까우면 자동으로 맞춰짐, Shift 로 자유 각도");
    case Tool::Arrow: return QStringLiteral("드래그로 화살표 (끝점이 머리)");
    case Tool::Text: return QStringLiteral("넣을 위치를 클릭 → 입력 → Enter · 크기는 휠, 글자 칸, 오른쪽 아래 손잡이 · 우클릭으로 테두리·색 변경 (Esc 취소)");
    case Tool::Fill: return QStringLiteral("드래그한 영역을 현재 색으로 채움 (가리기용)");
    }
    return QString();
}

// ── 되돌리기 ─────────────────────────────────────────────
void Canvas::pushUndo() {
    m_undo.append(Snapshot{m_items, m_img, m_imageEdited});
    if (m_undo.size() > 100)
        m_undo.removeFirst();
    m_redo.clear();
}

void Canvas::bump() {
    ++m_rev;
    update();
    emit changed();
}

void Canvas::undo() {
    if (m_edit->isVisible()) {
        cancelPending();
        return;
    }
    if (m_undo.isEmpty())
        return;
    m_redo.append(Snapshot{m_items, m_img, m_imageEdited});
    restore(m_undo.takeLast());
}

void Canvas::redo() {
    if (m_redo.isEmpty())
        return;
    m_undo.append(Snapshot{m_items, m_img, m_imageEdited});
    restore(m_redo.takeLast());
}

// 되돌리기/다시 실행 공통 — 이미지 크기가 달라졌으면 배율과 창까지 다시 맞춘다
void Canvas::restore(const Snapshot &s) {
    const bool sizeChanged = s.img.size() != m_img.size();
    m_items = s.items;
    m_img = s.img;
    m_imageEdited = s.imageEdited;
    clearRegion();
    select(-1);
    if (sizeChanged) {
        m_scaled = QPixmap();
        relayout();
    }
    bump();
    if (sizeChanged)
        emit imageResized(m_img.size());
}

void Canvas::deleteSelected() {
    if (m_sel < 0) {
        clearRegion();
        return;
    }
    pushUndo();
    m_items.removeAt(m_sel);
    select(-1);
    bump();
}

// 선택하면 그 항목의 색·굵기·글자 크기를 현재 값으로 가져와 툴바가 그것을 보여 준다
void Canvas::select(int idx) {
    if (m_sel == idx)
        return;
    m_sel = idx;
    if (idx >= 0 && idx < m_items.size()) {
        const Item &it = m_items[idx];
        if (it.color != m_color) {
            m_color = it.color;
            emit colorChanged(m_color);
        }
        if (it.type == Tool::Text) {
            if (it.textPx != m_textPx) {
                m_textPx = it.textPx;
                emit textPxChanged(m_textPx);
            }
        } else if (it.type != Tool::Fill && it.width != m_width) {
            m_width = it.width;
            emit lineWidthChanged(m_width);
        }
    }
    emit selectionChanged();
    update();
}

int Canvas::hitTest(const QPointF &img) const {
    const qreal tol = kHitTolPx / m_scale;
    for (int i = m_items.size() - 1; i >= 0; --i)   // 위에 그린 것부터
        if (Annot::hit(m_items[i], img, tol))
            return i;
    return -1;
}

QRectF Canvas::handleRect() const {
    if (m_sel < 0 || m_sel >= m_items.size() || m_items[m_sel].type != Tool::Text)
        return QRectF();
    const QRectF b = Annot::bounds(m_items[m_sel]);
    const QPointF br = toWidget(b.bottomRight()) + QPointF(4, 4);
    return QRectF(br.x() - kHandle / 2, br.y() - kHandle / 2, kHandle, kHandle);
}

bool Canvas::cancelPending() {
    if (m_edit->isVisible()) {
        m_committing = true;
        m_edit->hide();
        m_edit->clear();
        m_editIndex = -1;
        m_committing = false;
        setFocus();
        return true;
    }
    if (m_drawing) {
        m_drawing = false;
        update();
        return true;
    }
    if (m_hasRegion || m_regionDrag) {
        clearRegion();
        return true;
    }
    if (m_sel >= 0) {
        select(-1);
        return true;
    }
    return false;
}

// ── 텍스트 입력 ───────────────────────────────────────────
void Canvas::beginTextEdit(const QPointF &imgPos, int editIndex) {
    m_editIndex = editIndex;
    m_editPos = imgPos;
    if (editIndex >= 0) {
        m_edit->setText(m_items[editIndex].text);
        m_editPos = m_items[editIndex].p1;
        select(editIndex);   // 색·크기를 그 항목 값으로
    } else {
        m_edit->clear();
    }
    m_edit->show();
    placeTextEdit();
    m_edit->setFocus();
    m_edit->selectAll();
}

void Canvas::placeTextEdit() {
    if (!m_edit->isVisible())
        return;
    const int px = qMax(6, int(std::lround(m_textPx * m_scale)));
    QFont f = Annot::textFont(px);
    m_edit->setFont(f);
    m_edit->setStyleSheet(QString("QLineEdit { background: rgba(10,10,12,190); color: %1; border: 1px solid %2; "
                                  "border-radius: 4px; padding: 1px 4px; }")
                              .arg(m_color.name(), Theme::accent().name()));
    const QFontMetrics fm(f);
    const int w = qMax(140, fm.horizontalAdvance(m_edit->text()) + 40);
    const QPointF wp = toWidget(m_editPos);
    m_edit->setGeometry(int(wp.x()), int(wp.y()), qMin(w, qMax(140, width() - int(wp.x()) - 4)),
                        fm.height() + 6);
}

void Canvas::commitTextEdit() {
    if (m_committing || !m_edit->isVisible())
        return;
    m_committing = true;
    const QString text = m_edit->text().trimmed();
    m_edit->hide();
    if (m_editIndex >= 0) {
        if (text.isEmpty()) {
            pushUndo();
            m_items.removeAt(m_editIndex);
            select(-1);
            bump();
        } else {
            if (m_items[m_editIndex].text != text) {
                pushUndo();
                m_items[m_editIndex].text = text;
                bump();
            }
            select(m_editIndex);
        }
    } else if (!text.isEmpty()) {
        pushUndo();
        Item it;
        it.type = Tool::Text;
        it.p1 = m_editPos;
        it.p2 = m_editPos;
        it.color = m_color;
        it.text = text;
        it.textPx = m_textPx;
        it.outline = m_textOutline;
        m_items.append(it);
        select(m_items.size() - 1);   // 확정 직후 바로 크기·색을 바꾸거나 옮길 수 있게 선택 상태로
        bump();
    }
    m_editIndex = -1;
    m_edit->clear();
    m_committing = false;
    setFocus();
}

void Canvas::finishTextEdit() {
    if (m_edit->isVisible())
        commitTextEdit();
}

// ── 선택 영역 (Region 도구) ───────────────────────────────
QRectF Canvas::regionF() const {
    return QRectF(QPointF(qMin(m_rgA.x(), m_rgB.x()), qMin(m_rgA.y(), m_rgB.y())),
                  QPointF(qMax(m_rgA.x(), m_rgB.x()), qMax(m_rgA.y(), m_rgB.y())));
}

QPointF Canvas::clampToImage(const QPointF &p) const {
    if (m_img.isNull())
        return p;
    return QPointF(qBound(0.0, p.x(), qreal(m_img.width())), qBound(0.0, p.y(), qreal(m_img.height())));
}

// Shift: 정사각형. 현재점은 이미 이미지 안으로 잘라 두므로 짧은 변에 맞추면 밖으로 안 나간다.
void Canvas::squareRegion() {
    const qreal dx = m_rgB.x() - m_rgA.x(), dy = m_rgB.y() - m_rgA.y();
    const qreal side = qMin(qAbs(dx), qAbs(dy));
    m_rgB = QPointF(m_rgA.x() + (dx >= 0 ? side : -side), m_rgA.y() + (dy >= 0 ? side : -side));
}

void Canvas::syncMarquee() {
    const bool on = m_hasRegion || m_regionDrag;
    if (on && !m_marquee->isActive())
        m_marquee->start();
    else if (!on && m_marquee->isActive())
        m_marquee->stop();
}

void Canvas::clearRegion() {
    if (!m_hasRegion && !m_regionDrag && !m_regionMove)
        return;
    m_hasRegion = m_regionDrag = m_regionMove = false;
    syncMarquee();
    update();
}

// 선택 영역대로 이미지를 자른다 — 항목은 좌표만 옮겨 그대로 편집할 수 있다 (되돌리기 가능)
void Canvas::cropToRegion() {
    if (!m_hasRegion || m_img.isNull())
        return;
    finishTextEdit();
    const QRect r = regionF().toRect().intersected(m_img.rect());
    if (r.width() < 1 || r.height() < 1)
        return;
    pushUndo();
    QImage cropped = m_img.copy(r);
    cropped.setDevicePixelRatio(1.0);
    m_img = cropped;
    for (Item &it : m_items)
        it.move(QPointF(-r.x(), -r.y()));
    m_imageEdited = true;
    m_hasRegion = m_regionDrag = m_regionMove = false;
    syncMarquee();
    select(-1);
    m_scaled = QPixmap();
    relayout();
    bump();
    emit imageResized(m_img.size());
}

void Canvas::fillRegion() {
    if (!m_hasRegion)
        return;
    const QRectF r = regionF();
    if (r.width() < 1 || r.height() < 1)
        return;
    pushUndo();
    Item it;
    it.type = Tool::Fill;
    it.p1 = r.topLeft();
    it.p2 = r.bottomRight();
    it.color = m_color;
    m_items.append(it);
    clearRegion();
    bump();
}

void Canvas::borderRegion() {
    if (!m_hasRegion)
        return;
    const QRectF r = regionF();
    if (r.width() < 1 || r.height() < 1)
        return;
    pushUndo();
    Item it;
    it.type = Tool::Rect;
    it.p1 = r.topLeft();
    it.p2 = r.bottomRight();
    it.color = m_color;
    it.width = m_width;
    m_items.append(it);
    clearRegion();
    bump();
}

// ── 우클릭 메뉴 ───────────────────────────────────────────
void Canvas::showRegionMenu(const QPoint &globalPos) {
    QMenu menu(this);
    // 단축키 표기는 QMenu 의 탭 뒤 칸에 — Mview 자르기 메뉴와 같은 말투
    QAction *crop = menu.addAction(QStringLiteral("이 영역으로 자르기\tEnter"));
    menu.addSeparator();
    QAction *fill = menu.addAction(QStringLiteral("선택 영역 색 채우기"));
    QAction *border = menu.addAction(QStringLiteral("선택 영역 테두리 추가"));
    menu.addSeparator();
    QAction *clear = menu.addAction(QStringLiteral("선택 지우기\tEsc"));

    const QAction *picked = menu.exec(globalPos);
    if (picked == crop)
        cropToRegion();
    else if (picked == fill)
        fillRegion();
    else if (picked == border)
        borderRegion();
    else if (picked == clear)
        clearRegion();
}

void Canvas::showItemMenu(int idx, const QPoint &globalPos) {
    if (idx < 0 || idx >= m_items.size())
        return;
    const bool isText = m_items[idx].type == Tool::Text;
    const bool hasOutline = m_items[idx].outline;
    const QPointF anchor = m_items[idx].p1;

    QMenu menu(this);
    QAction *editText = nullptr, *outline = nullptr;
    if (isText) {
        editText = menu.addAction(QStringLiteral("텍스트 수정"));
        menu.addSeparator();
        outline = menu.addAction(QStringLiteral("테두리"));
        outline->setCheckable(true);
        outline->setChecked(hasOutline);
    }
    QAction *color = menu.addAction(QStringLiteral("색 변경…"));
    menu.addSeparator();
    QAction *del = menu.addAction(QStringLiteral("삭제\tDelete"));

    const QAction *picked = menu.exec(globalPos);
    if (!picked)
        return;
    if (picked == editText)
        beginTextEdit(anchor, idx);
    else if (picked == outline)
        setTextOutline(!hasOutline);
    else if (picked == color)
        emit colorPickRequested();
    else if (picked == del)
        deleteSelected();
}

void Canvas::contextMenuEvent(QContextMenuEvent *e) {
    if (m_img.isNull()) {
        e->ignore();
        return;
    }
    if (m_drawing || m_edit->isVisible()) {   // 그리는 중/입력 중이면 우클릭은 취소
        cancelPending();
        e->accept();
        return;
    }
    const int idx = hitTest(toImage(e->pos()));
    if (idx >= 0) {
        select(idx);
        showItemMenu(idx, e->globalPos());
    } else if (m_hasRegion) {
        showRegionMenu(e->globalPos());
    } else if (!cancelPending()) {
        e->ignore();
        return;
    }
    e->accept();
}

// ── 마우스 ────────────────────────────────────────────────
void Canvas::snapLine(QPointF &p2, const QPointF &p1, bool free) const {
    if (free)
        return;
    const qreal dx = p2.x() - p1.x(), dy = p2.y() - p1.y();
    const qreal t = qTan(qDegreesToRadians(8.0));
    if (qAbs(dy) <= qAbs(dx) * t)
        p2.setY(p1.y());
    else if (qAbs(dx) <= qAbs(dy) * t)
        p2.setX(p1.x());
}

void Canvas::updateCursor(const QPointF &imgPos) {
    if (m_resizing || handleRect().contains(toWidget(imgPos))) {
        setCursor(Qt::SizeFDiagCursor);
    } else if (m_tool == Tool::Region) {
        if (m_regionMove)
            setCursor(Qt::ClosedHandCursor);
        else
            setCursor(m_hasRegion && regionF().contains(imgPos) ? Qt::SizeAllCursor : Qt::CrossCursor);
    } else if (m_tool == Tool::Select) {
        if (m_moving)
            setCursor(Qt::ClosedHandCursor);
        else
            setCursor(hitTest(imgPos) >= 0 ? Qt::SizeAllCursor : Qt::ArrowCursor);
    } else if (m_tool == Tool::Text) {
        setCursor(Qt::IBeamCursor);
    } else {
        setCursor(Qt::CrossCursor);
    }
}

void Canvas::mousePressEvent(QMouseEvent *e) {
    if (m_img.isNull())
        return;
    const QPointF pos = toImage(e->position());
    if (e->button() == Qt::RightButton)
        return;   // 메뉴는 contextMenuEvent 에서 — 누를 때 선택/영역을 지우면 안 된다
    if (e->button() != Qt::LeftButton)
        return;
    if (m_edit->isVisible()) {
        commitTextEdit();
        if (m_tool == Tool::Text && !handleRect().contains(e->position()))
            return;   // 입력 확정 후 같은 클릭으로 새 입력을 시작하지 않는다
    }
    setFocus();

    // 선택된 텍스트의 오른쪽 아래 손잡이: 드래그로 크기 조절 (도구와 무관)
    if (handleRect().contains(e->position())) {
        pushUndo();
        m_resizing = true;
        m_rsStartW = qMax(1.0, Annot::textRect(m_items[m_sel]).width());
        m_rsStartPx = m_items[m_sel].textPx;
        setCursor(Qt::SizeFDiagCursor);
        return;
    }

    switch (m_tool) {
    case Tool::Region: {
        if (m_hasRegion && regionF().contains(pos)) {   // 안쪽을 잡으면 영역을 통째로 이동
            m_regionMove = true;
            m_regionGrab = pos - regionF().topLeft();
            setCursor(Qt::ClosedHandCursor);
        } else {
            m_regionDrag = true;
            m_hasRegion = false;
            m_rgA = m_rgB = clampToImage(pos);
            syncMarquee();
        }
        break;
    }
    case Tool::Select: {
        const int idx = hitTest(pos);
        select(idx);
        if (idx >= 0) {
            m_moving = true;
            m_movePushed = false;
            m_lastPos = pos;
            setCursor(Qt::ClosedHandCursor);
        }
        break;
    }
    case Tool::Text: {
        const int idx = hitTest(pos);
        if (idx >= 0 && m_items[idx].type == Tool::Text)
            beginTextEdit(pos, idx);
        else
            beginTextEdit(pos, -1);
        break;
    }
    default:
        m_drawing = true;
        m_cur = Item();
        m_cur.type = m_tool;
        m_cur.p1 = pos;
        m_cur.p2 = pos;
        m_cur.color = m_color;
        m_cur.width = m_width;
        break;
    }
    update();
}

void Canvas::mouseMoveEvent(QMouseEvent *e) {
    if (m_img.isNull())
        return;
    const QPointF pos = toImage(e->position());
    if (m_resizing && m_sel >= 0) {
        // 좌상단 고정, 텍스트 폭 비율로 글자 크기 환산 (Mview 텍스트 조각과 같은 방식)
        const qreal w = pos.x() - m_items[m_sel].p1.x();
        const int px = int(std::lround(m_rsStartPx * (w / m_rsStartW)));
        applyTextPx(px);
        return;
    }
    if (m_regionDrag) {
        m_rgB = clampToImage(pos);
        if (e->modifiers() & Qt::ShiftModifier)
            squareRegion();
        update();
        return;
    }
    if (m_regionMove) {
        const QSizeF sz = regionF().size();
        QPointF tl = pos - m_regionGrab;
        tl.setX(qBound(0.0, tl.x(), qMax(0.0, m_img.width() - sz.width())));
        tl.setY(qBound(0.0, tl.y(), qMax(0.0, m_img.height() - sz.height())));
        m_rgA = tl;
        m_rgB = tl + QPointF(sz.width(), sz.height());
        update();
        return;
    }
    if (m_drawing) {
        m_cur.p2 = pos;
        if (m_cur.type == Tool::Line || m_cur.type == Tool::Arrow)
            snapLine(m_cur.p2, m_cur.p1, e->modifiers() & Qt::ShiftModifier);
        update();
        return;
    }
    if (m_moving && m_sel >= 0) {
        const QPointF d = pos - m_lastPos;
        if (!d.isNull()) {
            if (!m_movePushed) {
                pushUndo();
                m_movePushed = true;
            }
            m_items[m_sel].move(d);
            m_lastPos = pos;
            bump();
        }
        return;
    }
    updateCursor(pos);
}

void Canvas::mouseReleaseEvent(QMouseEvent *e) {
    if (e->button() != Qt::LeftButton)
        return;
    if (m_regionDrag) {
        m_regionDrag = false;
        const QRectF r = regionF();
        m_hasRegion = r.width() >= 1.0 && r.height() >= 1.0;
        syncMarquee();
        updateCursor(toImage(e->position()));
        update();
        return;
    }
    if (m_regionMove) {
        m_regionMove = false;
        updateCursor(toImage(e->position()));
        return;
    }
    if (m_resizing) {
        m_resizing = false;
        updateCursor(toImage(e->position()));
        return;
    }
    if (m_moving) {
        m_moving = false;
        updateCursor(toImage(e->position()));
        return;
    }
    if (!m_drawing)
        return;
    m_drawing = false;
    const bool isLine = m_cur.type == Tool::Line || m_cur.type == Tool::Arrow;
    const qreal minPx = 3.0 / m_scale;
    const bool big = isLine ? QLineF(m_cur.p1, m_cur.p2).length() >= minPx
                            : (m_cur.rect().width() >= minPx && m_cur.rect().height() >= minPx);
    if (big) {
        pushUndo();
        m_items.append(m_cur);
        bump();
    } else {
        update();
    }
}

void Canvas::mouseDoubleClickEvent(QMouseEvent *e) {
    if (e->button() != Qt::LeftButton || m_tool != Tool::Select)
        return;
    const QPointF pos = toImage(e->position());
    const int idx = hitTest(pos);
    if (idx >= 0 && m_items[idx].type == Tool::Text) {
        m_moving = false;
        select(idx);
        beginTextEdit(pos, idx);
    }
}

// 휠: 입력 중이거나 선택된 텍스트는 글자 크기 10%씩, 선택된 도형은 굵기 1px 씩
void Canvas::wheelEvent(QWheelEvent *e) {
    const int delta = e->angleDelta().y();
    if (delta == 0) {
        e->ignore();
        return;
    }
    const bool up = delta > 0;
    const bool textTarget = m_edit->isVisible() || (m_sel >= 0 && m_items[m_sel].type == Tool::Text);
    if (textTarget) {
        int px = int(std::lround(m_textPx * (up ? 1.1 : 1 / 1.1)));
        if (px == m_textPx)
            px += up ? 1 : -1;
        applyTextPx(px);
        e->accept();
        return;
    }
    if (m_sel >= 0 && m_items[m_sel].type != Tool::Fill) {
        const int w = qBound(1, m_width + (up ? 1 : -1), 40);
        if (w != m_width) {
            setLineWidth(w);
            emit lineWidthChanged(w);
        }
        e->accept();
        return;
    }
    e->ignore();
}

void Canvas::keyPressEvent(QKeyEvent *e) {
    if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && m_hasRegion && !m_edit->isVisible()) {
        cropToRegion();   // Mview 자르기와 같은 키
        return;
    }
    if (m_sel >= 0 && !m_edit->isVisible()) {
        QPointF d;
        const qreal step = (e->modifiers() & Qt::ShiftModifier) ? 10 : 1;
        switch (e->key()) {
        case Qt::Key_Left: d = QPointF(-step, 0); break;
        case Qt::Key_Right: d = QPointF(step, 0); break;
        case Qt::Key_Up: d = QPointF(0, -step); break;
        case Qt::Key_Down: d = QPointF(0, step); break;
        default: break;
        }
        if (!d.isNull()) {
            pushUndo();
            m_items[m_sel].move(d);
            bump();
            return;
        }
    }
    e->ignore();
}

// ── 그리기 ────────────────────────────────────────────────
void Canvas::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(), Theme::stage());
    if (m_img.isNull())
        return;

    const QRectF dst(m_origin, QSizeF(m_img.size()) * m_scale);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.drawPixmap(dst, m_scaled, QRectF(QPointF(0, 0), QSizeF(m_scaled.size())));
    // 이미지 테두리 (배경과 구분)
    p.setPen(QPen(QColor(255, 255, 255, 30), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(dst.adjusted(-0.5, -0.5, 0.5, 0.5));

    p.save();
    p.setClipRect(dst);
    p.translate(m_origin);
    p.scale(m_scale, m_scale);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    Annot::paintAll(p, m_items);
    if (m_drawing)
        Annot::paint(p, m_cur);
    p.restore();

    paintRegion(p, dst);

    if (m_sel >= 0 && m_sel < m_items.size()) {
        const QRectF b = Annot::bounds(m_items[m_sel]);
        const QRectF wb(toWidget(b.topLeft()), toWidget(b.bottomRight()));
        QPen pen(Theme::accent(), 1.5, Qt::DashLine);
        pen.setCosmetic(true);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRect(wb.adjusted(-4, -4, 4, 4));
        const QRectF h = handleRect();
        if (!h.isNull()) {   // 텍스트: 오른쪽 아래 크기 조절 손잡이
            p.setPen(QPen(QColor(20, 20, 24), 1));
            p.setBrush(Theme::accent());
            p.drawRect(h);
        }
    }
}

// 선택 영역 오버레이 — 바깥 딤 + 행진 점선 + 코너 손잡이 + 실시간 크기 (Mview 자르기와 같은 모양)
void Canvas::paintRegion(QPainter &p, const QRectF &dst) {
    if (!m_hasRegion && !m_regionDrag)
        return;
    const QRectF r = regionF();
    if (r.width() < 0.5 || r.height() < 0.5)
        return;
    const QRectF wr = QRectF(toWidget(r.topLeft()), toWidget(r.bottomRight())).intersected(dst);

    const QColor dim(8, 8, 10, 166);
    p.fillRect(QRectF(dst.left(), dst.top(), dst.width(), wr.top() - dst.top()), dim);
    p.fillRect(QRectF(dst.left(), wr.bottom(), dst.width(), dst.bottom() - wr.bottom()), dim);
    p.fillRect(QRectF(dst.left(), wr.top(), wr.left() - dst.left(), wr.height()), dim);
    p.fillRect(QRectF(wr.right(), wr.top(), dst.right() - wr.right(), wr.height()), dim);

    p.setBrush(Qt::NoBrush);
    QPen under(QColor(0, 0, 0, 150), 2.0);
    under.setCosmetic(true);
    p.setPen(under);
    p.drawRect(wr);
    QPen dash(Qt::white, 1.5);
    dash.setCosmetic(true);
    dash.setDashPattern(QList<qreal>{5.0, 5.0});
    dash.setDashOffset(m_dashPhase);
    p.setPen(dash);
    p.drawRect(wr);

    const qreal hs = 10.0;
    const qreal xs[2] = {wr.left() - hs / 2, wr.right() - hs / 2};
    const qreal ys[2] = {wr.top() - hs / 2, wr.bottom() - hs / 2};
    p.setPen(Qt::NoPen);
    for (qreal x : xs) {
        for (qreal y : ys) {
            p.setBrush(QColor(0, 0, 0, 90));
            p.drawRoundedRect(QRectF(x + 1, y + 2, hs, hs), 2, 2);
            p.setBrush(Qt::white);
            p.drawRoundedRect(QRectF(x, y, hs, hs), 2, 2);
        }
    }

    const QString label = QString("%1 × %2 px").arg(qRound(r.width())).arg(qRound(r.height()));
    QFont f = font();
    f.setPixelSize(12);
    f.setBold(true);
    const QFontMetricsF fm(f);
    const qreal bw = fm.horizontalAdvance(label) + 20, bh = 26;
    qreal by = wr.bottom() + 8;
    if (by + bh > height() - 4)
        by = qMax(4.0, wr.bottom() - bh - 8);
    const qreal bx = qBound(4.0, wr.right() - bw, qMax(4.0, width() - bw - 4));
    QPainterPath bp;
    bp.addRoundedRect(QRectF(bx, by, bw, bh), 8, 8);
    p.fillPath(bp, QColor(18, 18, 22, 217));
    p.setPen(QPen(QColor(255, 255, 255, 31), 1));
    p.setBrush(Qt::NoBrush);
    p.drawPath(bp);
    p.setFont(f);
    p.setPen(QColor(0xf0, 0xf0, 0xf2));
    p.drawText(QRectF(bx, by, bw, bh), Qt::AlignCenter, label);
}
