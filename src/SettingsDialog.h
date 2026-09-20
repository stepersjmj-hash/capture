#pragma once

// 설정 대화상자 — 단축키 · 클립보드 감시 · 자동 복사 · 저장 폴더 · 자동 실행 · 자동 업데이트.
// 확인을 누르면 QSettings 에 쓰고 닫힌다. 실제 적용(단축키 재등록 등)은 App::applySettings.
#include <QDialog>

class QCheckBox;
class QKeySequenceEdit;
class QLineEdit;
class QSettings;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QSettings &settings, QWidget *parent = nullptr);
    void accept() override;

private:
    QSettings &m_s;
    QKeySequenceEdit *m_region = nullptr;
    QKeySequenceEdit *m_full = nullptr;
    QCheckBox *m_watch = nullptr;
    QCheckBox *m_autoCopy = nullptr;
    QCheckBox *m_autoStart = nullptr;
    QCheckBox *m_autoUpdate = nullptr;
    QLineEdit *m_dir = nullptr;
};
