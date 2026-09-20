#include "SettingsDialog.h"
#include "Defaults.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QSettings &settings, QWidget *parent) : QDialog(parent), m_s(settings) {
    setWindowTitle("Mcapture 설정");
    setMinimumWidth(520);
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(20, 18, 20, 16);
    v->setSpacing(14);

    // ── 단축키
    auto *keys = new QGroupBox("전역 단축키", this);
    auto *kf = new QFormLayout(keys);
    kf->setHorizontalSpacing(14);
    kf->setVerticalSpacing(8);
    m_region = new QKeySequenceEdit(keys);
    m_full = new QKeySequenceEdit(keys);
    for (QKeySequenceEdit *e : {m_region, m_full}) {
        e->setMaximumSequenceLength(1);
        e->setClearButtonEnabled(true);
    }
    m_region->setKeySequence(QKeySequence(
        m_s.value("hotkey/region", Defaults::regionKey().toString(QKeySequence::PortableText)).toString(),
        QKeySequence::PortableText));
    m_full->setKeySequence(QKeySequence(
        m_s.value("hotkey/full", Defaults::fullKey().toString(QKeySequence::PortableText)).toString(),
        QKeySequence::PortableText));
    kf->addRow("영역 캡처", m_region);
    kf->addRow("전체 화면 캡처", m_full);
    auto *note = new QLabel("칸을 누르고 조합을 입력합니다. 비우면 그 단축키는 쓰지 않습니다.\n"
                            "다른 프로그램이 이미 쓰는 조합은 등록되지 않으며, 확인 후 알려 드립니다.",
                            keys);
    note->setObjectName("dim");
    note->setWordWrap(true);
    kf->addRow(note);
    v->addWidget(keys);

    // ── 캡처
    auto *cap = new QGroupBox("캡처", this);
    auto *cv = new QVBoxLayout(cap);
    cv->setSpacing(8);
    m_watch = new QCheckBox(QString("OS 기본 캡처(%1)로 찍은 이미지도 편집 창으로 열기 (클립보드 감시)")
                                .arg(Defaults::osCaptureKeyText()),
                            cap);
    m_watch->setChecked(m_s.value("clipboard/watch", true).toBool());
    m_autoCopy = new QCheckBox("캡처하면 바로 클립보드에 복사 (편집 후 Ctrl+C 로 다시 복사)", cap);
    m_autoCopy->setChecked(m_s.value("capture/autoCopy", true).toBool());
    cv->addWidget(m_watch);
    cv->addWidget(m_autoCopy);
    auto *dirRow = new QHBoxLayout;
    auto *dl = new QLabel("저장 폴더", cap);
    m_dir = new QLineEdit(QDir::toNativeSeparators(m_s.value("save/dir", Defaults::saveDir()).toString()), cap);
    auto *browse = new QPushButton("찾아보기…", cap);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString d = QFileDialog::getExistingDirectory(this, "저장 폴더 선택", m_dir->text());
        if (!d.isEmpty())
            m_dir->setText(QDir::toNativeSeparators(d));
    });
    dirRow->addWidget(dl);
    dirRow->addWidget(m_dir, 1);
    dirRow->addWidget(browse);
    cv->addLayout(dirRow);
    v->addWidget(cap);

    // ── 일반
    auto *gen = new QGroupBox("일반", this);
    auto *gv = new QVBoxLayout(gen);
    gv->setSpacing(8);
#ifdef Q_OS_WIN
    m_autoStart = new QCheckBox("Windows 시작 시 자동 실행 (트레이에 상주)", gen);
    m_autoStart->setChecked(m_s.value("startup/run", false).toBool());
    gv->addWidget(m_autoStart);
#endif
    m_autoUpdate = new QCheckBox("시작할 때 새 버전 확인 (제작자 NAS 에서 읽기만 함 — 보내는 데이터 없음)", gen);
    m_autoUpdate->setChecked(m_s.value("update/auto", true).toBool());
    gv->addWidget(m_autoUpdate);
    v->addWidget(gen);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText("확인");
    buttons->button(QDialogButtonBox::Cancel)->setText("취소");
    buttons->button(QDialogButtonBox::Ok)->setDefault(true);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    v->addWidget(buttons);
}

void SettingsDialog::accept() {
    m_s.setValue("hotkey/region", m_region->keySequence().toString(QKeySequence::PortableText));
    m_s.setValue("hotkey/full", m_full->keySequence().toString(QKeySequence::PortableText));
    m_s.setValue("clipboard/watch", m_watch->isChecked());
    m_s.setValue("capture/autoCopy", m_autoCopy->isChecked());
    const QString dir = QDir::fromNativeSeparators(m_dir->text().trimmed());
    m_s.setValue("save/dir", dir.isEmpty() ? Defaults::saveDir() : dir);
    if (m_autoStart)
        m_s.setValue("startup/run", m_autoStart->isChecked());
    m_s.setValue("update/auto", m_autoUpdate->isChecked());
    m_s.sync();
    QDialog::accept();
}
