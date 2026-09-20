#include "EditorWindow.h"
#include "Canvas.h"
#include "Defaults.h"
#include "Icons.h"
#include "Theme.h"

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QCloseEvent>
#include <QColorDialog>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QSpinBox>
#include <QStatusBar>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace {

const QList<QColor> kPresets = {QColor(0xff, 0x3b, 0x30), QColor(0xe2, 0xb4, 0x64), QColor(0x34, 0xc7, 0x59),
                                QColor(0x3d, 0x8b, 0xff), QColor(0xff, 0xff, 0xff), QColor(0x11, 0x11, 0x11)};

QFrame *makeSep(QWidget *parent) {
    auto *f = new QFrame(parent);
    f->setObjectName("sep");
    f->setFrameShape(QFrame::NoFrame);
    f->setFixedWidth(1);
    return f;
}

} // namespace

EditorWindow::EditorWindow(QSettings &settings, const QImage &image, QWidget *parent)
    : QMainWindow(parent), m_settings(settings) {
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowIcon(QIcon(":/icons/app.svg"));

    auto *root = new QWidget(this);
    root->setObjectName("editorRoot");
    auto *v = new QVBoxLayout(root);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    m_canvas = new Canvas(root);
    m_canvas->setImage(image);
    m_canvas->setColor(QColor(m_settings.value("edit/color", kPresets[0].name()).toString()));
    m_canvas->setLineWidth(m_settings.value("edit/width", 3).toInt());
    m_canvas->setTextPx(m_settings.value("edit/textPx", 28).toInt());

    v->addWidget(buildToolbar());
    v->addWidget(m_canvas, 1);
    setCentralWidget(root);

    // 상태바: 크기 · 도구 안내 · (오른쪽) 저장/복사 알림
    m_sizeLabel = new QLabel(QString("%1 × %2").arg(image.width()).arg(image.height()), this);
    m_sizeLabel->setObjectName("status");
    m_hintLabel = new QLabel(this);
    m_hintLabel->setObjectName("status");
    m_msgLabel = new QLabel(this);
    m_msgLabel->setObjectName("statusOk");
    statusBar()->addWidget(m_sizeLabel);
    statusBar()->addWidget(makeSep(this));
    statusBar()->addWidget(m_hintLabel, 1);
    statusBar()->addPermanentWidget(m_msgLabel);
    statusBar()->setSizeGripEnabled(true);
    m_msgTimer = new QTimer(this);
    m_msgTimer->setSingleShot(true);
    connect(m_msgTimer, &QTimer::timeout, this, [this] { m_msgLabel->clear(); });

    connect(m_canvas, &Canvas::changed, this, [this] {
        updateActions();
        updateTitle();
    });
    connect(m_canvas, &Canvas::selectionChanged, this, &EditorWindow::updateActions);
    connect(m_canvas, &Canvas::hint, m_hintLabel, &QLabel::setText);

    // 단축키
    auto act = [this](const QList<QKeySequence> &keys, auto fn) {
        auto *a = new QAction(this);
        a->setShortcuts(keys);
        a->setShortcutContext(Qt::WindowShortcut);
        connect(a, &QAction::triggered, this, fn);
        addAction(a);
    };
    act({QKeySequence::Undo}, [this] { m_canvas->undo(); });
    act({QKeySequence::Redo, QKeySequence("Ctrl+Y")}, [this] { m_canvas->redo(); });
    act({QKeySequence::Copy}, [this] { copyImage(); });
    act({QKeySequence::Save}, [this] { save(); });
    act({QKeySequence("Ctrl+Shift+S")}, [this] { saveAs(); });
    act({QKeySequence::Close}, [this] { close(); });
    act({QKeySequence(Qt::Key_Escape)}, [this] { onEscape(); });
    act({QKeySequence(Qt::Key_Delete), QKeySequence(Qt::Key_Backspace)}, [this] { m_canvas->deleteSelected(); });
    const struct { Qt::Key key; Tool tool; } toolKeys[] = {
        {Qt::Key_V, Tool::Select}, {Qt::Key_R, Tool::Rect}, {Qt::Key_L, Tool::Line},
        {Qt::Key_A, Tool::Arrow},  {Qt::Key_T, Tool::Text}, {Qt::Key_F, Tool::Fill},
    };
    for (const auto &tk : toolKeys)
        act({QKeySequence(tk.key)}, [this, t = tk.tool] {
            if (auto *b = m_tools->button(int(t)))
                b->click();
        });

    // 기본 도구: 사각형
    if (auto *b = m_tools->button(int(Tool::Rect)))
        b->setChecked(true);
    m_canvas->setTool(Tool::Rect);
    updateChips();
    updateActions();
    updateTitle();

    // 창 크기: 이미지 원본 크기(+크롬), 화면의 92% 를 넘지 않게
    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QRect avail = screen ? screen->availableGeometry() : QRect(0, 0, 1280, 800);
    QSize want = image.size() + QSize(24, 24 + 74 + 28);
    want.setWidth(qBound(760, want.width(), int(avail.width() * 0.92)));
    want.setHeight(qBound(440, want.height(), int(avail.height() * 0.92)));
    resize(want);
    move(avail.center() - QPoint(want.width() / 2, want.height() / 2));
}

QToolButton *EditorWindow::toolButton(char16_t glyph, const QString &label, const QString &tip, bool checkable) {
    auto *b = new QToolButton(this);
    b->setProperty("cls", "tool");
    b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    b->setIconSize(QSize(20, 20));
    b->setIcon(Icons::glyph(glyph));
    b->setText(label);
    b->setToolTip(tip);
    b->setCheckable(checkable);
    b->setAutoRaise(true);
    b->setFocusPolicy(Qt::NoFocus);   // 키 입력은 캔버스가 받는다
    b->setMinimumWidth(qMax(46, b->fontMetrics().horizontalAdvance(label) + 18));   // 라벨 말줄임 방지
    return b;
}

QWidget *EditorWindow::buildToolbar() {
    auto *row = new QWidget(this);
    row->setObjectName("toolRow");
    auto *h = new QHBoxLayout(row);
    h->setContentsMargins(8, 4, 8, 4);
    h->setSpacing(2);

    // 도구
    m_tools = new QButtonGroup(this);
    m_tools->setExclusive(true);
    const struct { Tool tool; char16_t glyph; const char *label; const char *key; } defs[] = {
        {Tool::Select, Icons::kSelect, "선택", "V"},   {Tool::Rect, Icons::kRect, "사각형", "R"},
        {Tool::Line, Icons::kLine, "밑줄", "L"},       {Tool::Arrow, Icons::kArrow, "화살표", "A"},
        {Tool::Text, Icons::kText, "텍스트", "T"},     {Tool::Fill, Icons::kFill, "채우기", "F"},
    };
    for (const auto &d : defs) {
        QToolButton *b = toolButton(d.glyph, QString::fromUtf8(d.label),
                                    QString("%1 (%2)").arg(QString::fromUtf8(d.label), d.key), true);
        m_tools->addButton(b, int(d.tool));
        h->addWidget(b);
    }
    connect(m_tools, &QButtonGroup::idClicked, this, [this](int id) { m_canvas->setTool(Tool(id)); });
    h->addWidget(makeSep(row));

    // 색상 칩 + 사용자 색
    for (const QColor &c : kPresets) {
        auto *chip = new QToolButton(row);
        chip->setProperty("cls", "chip");
        chip->setCheckable(true);
        chip->setAutoExclusive(false);
        chip->setFocusPolicy(Qt::NoFocus);
        chip->setStyleSheet(QString("QToolButton { background: %1; }").arg(c.name()));
        chip->setToolTip(c.name());
        connect(chip, &QToolButton::clicked, this, [this, c] { applyColor(c); });
        m_chips.append(chip);
        m_chipColors.append(c);
        h->addSpacing(2);
        h->addWidget(chip);
    }
    h->addSpacing(2);
    m_customChip = toolButton(Icons::kPalette, "색상", "다른 색 선택…", false);
    connect(m_customChip, &QToolButton::clicked, this, [this] {
        const QColor c = QColorDialog::getColor(m_canvas->color(), this, "색 선택");
        if (c.isValid())
            applyColor(c);
    });
    h->addWidget(m_customChip);
    h->addWidget(makeSep(row));

    // 굵기 · 글자 크기
    auto *wl = new QLabel("굵기", row);
    wl->setObjectName("dim");
    m_width = new QSpinBox(row);
    m_width->setRange(1, 40);
    m_width->setSuffix(" px");
    m_width->setValue(m_canvas->lineWidth());
    m_width->setFocusPolicy(Qt::ClickFocus);
    m_width->setToolTip("선·사각형·화살표 굵기");
    connect(m_width, &QSpinBox::valueChanged, this, [this](int v) {
        m_canvas->setLineWidth(v);
        m_settings.setValue("edit/width", v);
        m_canvas->setFocus();
    });
    auto *tl = new QLabel("글자", row);
    tl->setObjectName("dim");
    m_textPx = new QSpinBox(row);
    m_textPx->setRange(8, 400);
    m_textPx->setSuffix(" px");
    m_textPx->setValue(m_canvas->textPx());
    m_textPx->setFocusPolicy(Qt::ClickFocus);
    m_textPx->setToolTip("텍스트 크기");
    connect(m_textPx, &QSpinBox::valueChanged, this, [this](int v) {
        m_canvas->setTextPx(v);
        m_settings.setValue("edit/textPx", v);
    });
    h->addSpacing(4);
    h->addWidget(wl);
    h->addWidget(m_width);
    h->addSpacing(8);
    h->addWidget(tl);
    h->addWidget(m_textPx);
    h->addSpacing(4);
    h->addWidget(makeSep(row));

    // 되돌리기 · 삭제
    m_undoBtn = toolButton(Icons::kUndo, "되돌리기", "되돌리기 (Ctrl+Z)", false);
    m_redoBtn = toolButton(Icons::kRedo, "다시실행", "다시 실행 (Ctrl+Y)", false);
    m_delBtn = toolButton(Icons::kDelete, "삭제", "선택한 항목 삭제 (Delete)", false);
    connect(m_undoBtn, &QToolButton::clicked, this, [this] { m_canvas->undo(); });
    connect(m_redoBtn, &QToolButton::clicked, this, [this] { m_canvas->redo(); });
    connect(m_delBtn, &QToolButton::clicked, this, [this] { m_canvas->deleteSelected(); });
    h->addWidget(m_undoBtn);
    h->addWidget(m_redoBtn);
    h->addWidget(m_delBtn);
    h->addStretch(1);

    // 내보내기
    auto *copyBtn = toolButton(Icons::kCopy, "복사", "클립보드에 복사 (Ctrl+C)", false);
    auto *saveBtn = toolButton(Icons::kSave, "저장", "저장 폴더에 PNG 로 저장 (Ctrl+S)", false);
    auto *saveAsBtn = toolButton(Icons::kSaveAs, "다른 이름", "다른 이름으로 저장 (Ctrl+Shift+S)", false);
    m_folderBtn = toolButton(Icons::kFolder, "폴더", "저장 폴더 열기", false);
    auto *settingsBtn = toolButton(Icons::kSettings, "설정", "단축키·저장 폴더 등 설정", false);
    connect(copyBtn, &QToolButton::clicked, this, [this] { copyImage(); });
    connect(saveBtn, &QToolButton::clicked, this, [this] { save(); });
    connect(saveAsBtn, &QToolButton::clicked, this, [this] { saveAs(); });
    connect(m_folderBtn, &QToolButton::clicked, this, [this] { openFolder(); });
    connect(settingsBtn, &QToolButton::clicked, this, [this] { emit settingsRequested(); });
    h->addWidget(copyBtn);
    h->addWidget(saveBtn);
    h->addWidget(saveAsBtn);
    h->addWidget(m_folderBtn);
    h->addWidget(makeSep(row));
    h->addWidget(settingsBtn);
    return row;
}

void EditorWindow::applyColor(const QColor &c) {
    m_canvas->setColor(c);
    m_settings.setValue("edit/color", c.name());
    updateChips();
}

void EditorWindow::updateChips() {
    const QColor cur = m_canvas->color();
    bool preset = false;
    for (int i = 0; i < m_chips.size(); ++i) {
        const bool on = m_chipColors[i] == cur;
        m_chips[i]->setChecked(on);
        preset = preset || on;
    }
    // 프리셋이 아니면 팔레트 아이콘을 현재 색으로 물들여 표시
    m_customChip->setIcon(preset ? Icons::glyph(Icons::kPalette) : Icons::glyph(Icons::kPalette, cur, cur));
}

void EditorWindow::updateActions() {
    m_undoBtn->setEnabled(m_canvas->canUndo());
    m_redoBtn->setEnabled(m_canvas->canRedo());
    m_delBtn->setEnabled(m_canvas->hasSelection());
}

bool EditorWindow::dirty() const {
    const int rev = m_canvas->revision();
    return m_canvas->hasItems() && rev != m_savedRev && rev != m_copiedRev;
}

void EditorWindow::updateTitle() {
    const QImage &img = m_canvas->image();
    setWindowTitle(QString("Mcapture — %1 × %2%3").arg(img.width()).arg(img.height()).arg(dirty() ? " •" : ""));
}

void EditorWindow::flash(const QString &msg) {
    m_msgLabel->setText(msg);
    m_msgTimer->start(5000);
}

QString EditorWindow::saveDir() const {
    return m_settings.value("save/dir", Defaults::saveDir()).toString();
}

QString EditorWindow::uniquePath(const QString &dir, const QString &ext) const {
    const QString base = "Mcapture_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString path = dir + "/" + base + "." + ext;
    for (int n = 2; QFileInfo::exists(path); ++n)
        path = dir + "/" + base + "_" + QString::number(n) + "." + ext;
    return path;
}

void EditorWindow::copyImage() {
    m_canvas->finishTextEdit();
    QApplication::clipboard()->setImage(m_canvas->flattened());
    emit copiedToClipboard();
    m_copiedRev = m_canvas->revision();
    flash("클립보드에 복사됨");
    updateTitle();
}

void EditorWindow::save() {
    m_canvas->finishTextEdit();
    const QString dir = saveDir();
    if (!QDir().mkpath(dir)) {
        QMessageBox::warning(this, "저장 실패", "저장 폴더를 만들 수 없습니다:\n" + QDir::toNativeSeparators(dir));
        return;
    }
    const QString path = uniquePath(dir, "png");
    if (!m_canvas->flattened().save(path, "PNG")) {
        QMessageBox::warning(this, "저장 실패", "파일을 쓰지 못했습니다:\n" + QDir::toNativeSeparators(path));
        return;
    }
    m_lastSaved = path;
    m_savedRev = m_canvas->revision();
    flash("저장됨: " + QDir::toNativeSeparators(path));
    updateTitle();
}

void EditorWindow::saveAs() {
    m_canvas->finishTextEdit();
    const QString dir = saveDir();
    QDir().mkpath(dir);
    QString filter;
    const QString path = QFileDialog::getSaveFileName(this, "다른 이름으로 저장", uniquePath(dir, "png"),
                                                      "PNG 이미지 (*.png);;JPEG 이미지 (*.jpg)", &filter);
    if (path.isEmpty())
        return;
    const bool jpg = path.endsWith(".jpg", Qt::CaseInsensitive) || path.endsWith(".jpeg", Qt::CaseInsensitive);
    const QImage img = m_canvas->flattened();
    const bool ok = jpg ? img.convertToFormat(QImage::Format_RGB32).save(path, "JPEG", 92) : img.save(path, "PNG");
    if (!ok) {
        QMessageBox::warning(this, "저장 실패", "파일을 쓰지 못했습니다:\n" + QDir::toNativeSeparators(path));
        return;
    }
    m_lastSaved = path;
    m_savedRev = m_canvas->revision();
    flash("저장됨: " + QDir::toNativeSeparators(path));
    updateTitle();
}

void EditorWindow::openFolder() {
#ifdef Q_OS_WIN
    if (!m_lastSaved.isEmpty() && QFileInfo::exists(m_lastSaved)) {
        QProcess::startDetached("explorer", {"/select,", QDir::toNativeSeparators(m_lastSaved)});
        return;
    }
#endif
    const QString dir = m_lastSaved.isEmpty() ? saveDir() : QFileInfo(m_lastSaved).absolutePath();
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void EditorWindow::onEscape() {
    if (m_canvas->cancelPending())
        return;
    close();
}

void EditorWindow::closeEvent(QCloseEvent *e) {
    m_canvas->finishTextEdit();
    if (dirty()) {
        QMessageBox box(this);
        box.setWindowTitle("Mcapture");
        box.setIcon(QMessageBox::Question);
        box.setText("편집한 내용을 저장하거나 복사하지 않았습니다.");
        box.setInformativeText("그래도 닫을까요?");
        QPushButton *closeBtn = box.addButton("닫기", QMessageBox::DestructiveRole);
        QPushButton *cancelBtn = box.addButton("취소", QMessageBox::RejectRole);
        box.setDefaultButton(cancelBtn);
        box.exec();
        if (box.clickedButton() != closeBtn) {
            e->ignore();
            return;
        }
    }
    e->accept();
}
