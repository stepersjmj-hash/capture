#pragma once

// 편집 창 — 캡처 직후 뜨는 창. 툴바(도구·색·굵기·되돌리기·복사·저장) + 캔버스 + 상태바.
#include <QHash>
#include <QImage>
#include <QList>
#include <QMainWindow>

class Canvas;
class QButtonGroup;
class QLabel;
class QSettings;
class QSpinBox;
class QTimer;
class QToolButton;

class EditorWindow : public QMainWindow {
    Q_OBJECT
public:
    EditorWindow(QSettings &settings, const QImage &image, QWidget *parent = nullptr);

    bool hasEdits() const;
    // 다른 이미지로 교체 (스니핑 도구가 전체 화면 뒤에 영역을 보낼 때 새 창 대신 씀)
    void replaceImage(const QImage &image);

signals:
    void copiedToClipboard(const QImage &image);   // 클립보드 감시가 우리 복사를 새 캡처로 오인하지 않게
    void settingsRequested();

protected:
    void closeEvent(QCloseEvent *e) override;

private:
    QWidget *buildToolbar();
    QToolButton *toolButton(char16_t glyph, const QString &label, const QString &tip, bool checkable);
    void applyColor(const QColor &c);
    void updateChips();
    void updateActions();
    void updateTitle();
    void fitToImage(const QSize &imageSize);
    bool dirty() const;
    void copyImage();
    void save();
    void saveAs();
    void openFolder();
    void onEscape();
    QString saveDir() const;
    QString uniquePath(const QString &dir, const QString &ext) const;
    void flash(const QString &msg);

    QSettings &m_settings;
    Canvas *m_canvas = nullptr;
    QButtonGroup *m_tools = nullptr;
    QList<QToolButton *> m_chips;
    QList<QColor> m_chipColors;
    QToolButton *m_customChip = nullptr;
    QSpinBox *m_width = nullptr;
    QSpinBox *m_textPx = nullptr;
    QToolButton *m_undoBtn = nullptr, *m_redoBtn = nullptr, *m_delBtn = nullptr, *m_folderBtn = nullptr;
    QLabel *m_sizeLabel = nullptr, *m_hintLabel = nullptr, *m_msgLabel = nullptr;
    QTimer *m_msgTimer = nullptr;
    QString m_lastSaved;
    int m_savedRev = -1, m_copiedRev = -1;
};
