#include "App.h"
#include "CaptureOverlay.h"
#include "ClipboardWatch.h"
#include "Defaults.h"
#include "EditorWindow.h"
#include "HotKey.h"
#include "Icons.h"
#include "Log.h"
#include "Platform.h"
#include "SettingsDialog.h"
#include "Updater.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QCursor>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QScreen>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QTimer>

App::App(QSettings &settings, QObject *parent) : QObject(parent), m_settings(settings) {
    m_hkRegion = new HotKey(this);
    m_hkFull = new HotKey(this);
    connect(m_hkRegion, &HotKey::activated, this, [this] { captureRegion(0); });
    connect(m_hkFull, &HotKey::activated, this, [this] { captureFull(0); });

    m_watch = new ClipboardWatch(this);
    connect(m_watch, &ClipboardWatch::imageArrived, this, &App::onClipboardImage);
    m_pendingTimer.setSingleShot(true);
    m_pendingTimer.setInterval(8000);
    connect(&m_pendingTimer, &QTimer::timeout, this, [this] {
        mlog("clipboard: held full-size image timed out -> open");
        const QImage img = m_pendingClip;
        m_pendingClip = QImage();
        openEditor(img, false);
    });

    buildTray();
    setupUpdater();
    applySettings(false);

    if (!m_settings.value("app/firstRunShown", false).toBool()) {
        m_settings.setValue("app/firstRunShown", true);
        QTimer::singleShot(1200, this, [this] {
            m_tray->showMessage("Mcapture 가 트레이에서 실행 중입니다",
                                QString("영역 캡처: %1\n%2 로 찍은 것도 편집 창으로 열립니다.")
                                    .arg(keyText(m_hkRegion), Defaults::osCaptureKeyText()),
                                QSystemTrayIcon::Information, 6000);
        });
    }
}

// ── 트레이 ────────────────────────────────────────────────
void App::buildTray() {
    m_menu = new QMenu;
    m_actRegion = m_menu->addAction(Icons::glyph(Icons::kCrop), "영역 캡처", this, [this] { captureRegion(200); });
    m_actFull = m_menu->addAction(Icons::glyph(Icons::kMonitor), "전체 화면 캡처", this, [this] { captureFull(250); });
    m_menu->addAction(Icons::glyph(Icons::kPaste), "클립보드 이미지 편집", this, &App::openClipboardImage);
    m_menu->addSeparator();
    m_actWatch = m_menu->addAction(QString("%1 캡처를 자동으로 열기").arg(Defaults::osCaptureKeyText()));
    m_actWatch->setCheckable(true);
    connect(m_actWatch, &QAction::toggled, this, [this](bool on) {
        m_settings.setValue("clipboard/watch", on);
        m_watch->setEnabled(on);
    });
    m_menu->addAction(Icons::glyph(Icons::kSettings), "설정…", this, &App::showSettings);
    m_menu->addSeparator();
    m_actUpdateNow = m_menu->addAction("업데이트 확인", this, [this] { m_updater->check(true); });
    m_actAutoUpdate = m_menu->addAction("시작할 때 자동으로 업데이트 확인");
    m_actAutoUpdate->setCheckable(true);
    connect(m_actAutoUpdate, &QAction::toggled, this, [this](bool on) { m_updater->setAuto(on); });
    m_menu->addAction(Icons::glyph(Icons::kInfo), QString("Mcapture v%1 정보").arg(APP_VERSION), this, &App::showAbout);
    m_menu->addSeparator();
    m_menu->addAction(Icons::glyph(Icons::kPower), "종료", qApp, &QCoreApplication::quit);

    m_tray = new QSystemTrayIcon(QIcon(":/icons/app.svg"), this);
    m_tray->setContextMenu(m_menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::DoubleClick)
            captureRegion(250);
        else if (r == QSystemTrayIcon::Trigger)
            m_menu->popup(QCursor::pos());
    });
    connect(m_tray, &QSystemTrayIcon::messageClicked, this, [this] {
        if (m_updateClickPending) {
            m_updateClickPending = false;
            m_updater->downloadAndInstall();
        }
    });
    m_tray->show();
}

QString App::keyText(const HotKey *hk) const {
    const QKeySequence seq = hk->sequence();
    if (seq.isEmpty())
        return "없음";
    return seq.toString(QKeySequence::NativeText) + (hk->isRegistered() ? "" : " (등록 실패)");
}

void App::refreshMenu() {
    m_actRegion->setText("영역 캡처\t" + keyText(m_hkRegion));
    m_actFull->setText("전체 화면 캡처\t" + keyText(m_hkFull));
    {
        QSignalBlocker b1(m_actWatch), b2(m_actAutoUpdate);
        m_actWatch->setChecked(m_watch->enabled());
        m_actAutoUpdate->setChecked(m_updater->autoEnabled());
    }
    m_tray->setToolTip(QString("Mcapture v%1 — 영역 캡처 %2").arg(APP_VERSION, keyText(m_hkRegion)));
}

// ── 설정 적용 ─────────────────────────────────────────────
void App::applySettings(bool interactive) {
    QStringList failed;
    const QKeySequence region(
        m_settings.value("hotkey/region", Defaults::regionKey().toString(QKeySequence::PortableText)).toString(),
        QKeySequence::PortableText);
    const QKeySequence full(
        m_settings.value("hotkey/full", Defaults::fullKey().toString(QKeySequence::PortableText)).toString(),
        QKeySequence::PortableText);
    if (!m_hkRegion->setSequence(region) && !region.isEmpty())
        failed << "영역 캡처: " + region.toString(QKeySequence::NativeText);
    if (!m_hkFull->setSequence(full) && !full.isEmpty())
        failed << "전체 화면 캡처: " + full.toString(QKeySequence::NativeText);

    m_watch->setEnabled(m_settings.value("clipboard/watch", true).toBool());

#ifdef Q_OS_WIN
    {
        QSettings run("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
        if (m_settings.value("startup/run", false).toBool())
            run.setValue("Mcapture", "\"" + QDir::toNativeSeparators(QCoreApplication::applicationFilePath()) + "\"");
        else
            run.remove("Mcapture");
    }
#elif defined(Q_OS_MACOS)
    Platform::setLoginItem(m_settings.value("startup/run", false).toBool());
#endif
    refreshMenu();

    if (!failed.isEmpty()) {
        const QString msg = "다른 프로그램이 이미 사용 중이라 등록하지 못했습니다:\n" + failed.join("\n") +
                            "\n\n설정에서 다른 조합으로 바꿔 주세요.";
        if (interactive)
            QMessageBox::warning(nullptr, "단축키 등록 실패", msg);
        else
            m_tray->showMessage("단축키 등록 실패", msg, QSystemTrayIcon::Warning, 8000);
    }
}

void App::showSettings() {
    static SettingsDialog *open = nullptr;
    if (open) {
        open->raise();
        open->activateWindow();
        return;
    }
    SettingsDialog dlg(m_settings);
    open = &dlg;
    const int r = dlg.exec();
    open = nullptr;
    if (r == QDialog::Accepted)
        applySettings(true);
}

// ── 캡처 ─────────────────────────────────────────────────
void App::captureRegion(int delayMs) {
    if (RegionCapture::active())
        return;
    RegionCapture::start([this](const QImage &img) { openEditor(img, true); }, delayMs);
}

void App::captureFull(int delayMs) {
    mlog("captureFull requested");
    QTimer::singleShot(delayMs, this, [this] {
        const QImage img = RegionCapture::grabFullScreen();
        if (img.isNull())
            m_tray->showMessage("캡처 실패", "화면을 읽지 못했습니다.", QSystemTrayIcon::Warning, 4000);
        else
            openEditor(img, true);
    });
}

void App::openClipboardImage() {
    const QMimeData *mime = QApplication::clipboard()->mimeData();
    const QImage img = (mime && mime->hasImage()) ? QApplication::clipboard()->image() : QImage();
    if (img.isNull()) {
        m_tray->showMessage("클립보드에 이미지가 없습니다", "먼저 화면을 캡처하거나 이미지를 복사하세요.",
                            QSystemTrayIcon::Information, 4000);
        return;
    }
    openEditor(img, false);
}

// 화면 하나 또는 전체 데스크톱과 같은 픽셀 크기인가 (스니핑 도구의 선행 복사본 판별)
bool App::isScreenSized(const QSize &size) const {
    QRect virt;
    for (QScreen *s : QGuiApplication::screens()) {
        const qreal dpr = s->devicePixelRatio();
        const QSize px(qRound(s->geometry().width() * dpr), qRound(s->geometry().height() * dpr));
        if (px == size)
            return true;
        virt |= QRect(QPoint(qRound(s->geometry().x() * dpr), qRound(s->geometry().y() * dpr)), px);
    }
    return virt.size() == size;
}

void App::onClipboardImage(const QImage &img) {
    if (isScreenSized(img.size())) {
        mlog(QString("clipboard: full-size %1x%2 held").arg(img.width()).arg(img.height()));
        m_pendingClip = img;
        m_pendingTimer.start();
        return;
    }
    if (m_pendingTimer.isActive()) {   // 전체 화면 뒤에 온 영역 = 스니핑 도구 결과, 전체는 버린다
        mlog("clipboard: dropped held full-size image");
        m_pendingTimer.stop();
        m_pendingClip = QImage();
    }
    if (m_lastClipWin && m_lastClipTime.isValid() && m_lastClipTime.elapsed() < 20000 && !m_lastClipWin->hasEdits()) {
        mlog(QString("clipboard: replace image in last window %1x%2").arg(img.width()).arg(img.height()));
        m_lastClipWin->replaceImage(img);
        m_lastClipTime.restart();
        m_lastClipWin->raise();
        m_lastClipWin->activateWindow();
        return;
    }
    openEditor(img, false);
}

void App::openEditor(const QImage &img, bool fromCapture) {
    if (img.isNull())
        return;
    mlog(QString("openEditor %1x%2 fromCapture=%3").arg(img.width()).arg(img.height()).arg(fromCapture));
    if (fromCapture && m_settings.value("capture/autoCopy", true).toBool()) {
        QApplication::clipboard()->setImage(img);
        m_watch->ignoreCurrent(img);
    }
    auto *w = new EditorWindow(m_settings, img);
    connect(w, &EditorWindow::copiedToClipboard, m_watch, [this](const QImage &img) { m_watch->ignoreCurrent(img); });
    connect(w, &EditorWindow::settingsRequested, this, &App::showSettings);
    connect(w, &EditorWindow::updateCheckRequested, this, [this] {
        if (m_updater)
            m_updater->check(true);
    });
    connect(w, &EditorWindow::aboutRequested, this, &App::showAbout);
    if (!fromCapture) {
        m_lastClipWin = w;
        m_lastClipTime.restart();
    }
    w->show();
    w->raise();
    w->activateWindow();
}

// ── 명령줄 ───────────────────────────────────────────────
void App::handleArgs(const QStringList &args, bool fromOtherInstance) {
    bool handled = false;
    mlog(QString("handleArgs [%1] other=%2").arg(args.join(' ')).arg(fromOtherInstance));
    for (int i = 0; i < args.size(); ++i) {
        const QString a = args[i];
        if (a == "--region") {
            captureRegion(fromOtherInstance ? 0 : 300);
            handled = true;
        } else if (a == "--full") {
            captureFull(fromOtherInstance ? 0 : 300);
            handled = true;
        } else if (a == "--settings") {
            QTimer::singleShot(0, this, &App::showSettings);
            handled = true;
        } else if (a == "--quit") {
            QTimer::singleShot(0, qApp, &QCoreApplication::quit);
            handled = true;
        } else {
            const QString path = (a == "--edit" && i + 1 < args.size()) ? args[++i] : a;
            if (path.startsWith("--"))
                continue;
            QImageReader reader(path);
            reader.setAutoTransform(true);
            const QImage img = reader.read();
            if (img.isNull())
                m_tray->showMessage("열 수 없는 이미지", QDir::toNativeSeparators(path), QSystemTrayIcon::Warning, 4000);
            else
                openEditor(img, false);
            handled = true;
        }
    }
    if (!handled && fromOtherInstance)
        m_tray->showMessage("Mcapture 는 이미 실행 중입니다",
                            QString("트레이 아이콘에서 사용하세요. 영역 캡처: %1").arg(keyText(m_hkRegion)),
                            QSystemTrayIcon::Information, 4000);
}

// ── 업데이트 · 정보 ───────────────────────────────────────
void App::setupUpdater() {
    m_updater = new Updater(&m_settings, this);
    connect(m_updater, &Updater::updateFound, this, [this](const QString &v, bool manual) {
        if (manual) {
            const auto r = QMessageBox::question(nullptr, "새 버전",
                                                 QString("새 버전 v%1 이 있습니다. 지금 받아서 다시 시작할까요?").arg(v),
                                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
            if (r == QMessageBox::Yes)
                m_updater->downloadAndInstall();
        } else {
            m_updateClickPending = true;
            m_tray->showMessage(QString("Mcapture v%1 업데이트").arg(v), "이 알림을 누르면 받아서 자동으로 다시 시작합니다.",
                                QSystemTrayIcon::Information, 10000);
        }
    });
    connect(m_updater, &Updater::upToDate, this, [](const QString &cur) {
        QMessageBox::information(nullptr, "업데이트 확인", QString("현재 v%1 이 최신 버전입니다.").arg(cur));
    });
    connect(m_updater, &Updater::checkFailed, this, [] {
        QMessageBox::warning(nullptr, "업데이트 확인", "NAS 에 연결하지 못했습니다. 잠시 뒤 다시 시도해 주세요.");
    });
    connect(m_updater, &Updater::installReady, this, [] { QCoreApplication::quit(); });
    connect(m_updater, &Updater::installFailed, this, [this](const QString &e) {
        m_tray->showMessage("업데이트 실패", e, QSystemTrayIcon::Warning, 6000);
    });
    if (m_updater->autoEnabled())
        QTimer::singleShot(4000, m_updater, [this] { m_updater->check(false); });
}

void App::showAbout() {
    QMessageBox::about(nullptr, "Mcapture 정보",
                       QString("<b>Mcapture</b> v%1<br>화면 캡처 + 간단 편집 (Windows · macOS)<br><br>"
                               "영역 캡처 <b>%2</b> · 전체 화면 <b>%3</b><br>"
                               "%4 로 찍은 것도 편집 창으로 열립니다.<br><br>"
                               "광고·수집·텔레메트리 없음. 네트워크는 제작자 NAS 에서 새 버전을 읽어 오는 "
                               "자동 업데이트(HTTPS)뿐입니다.<br>"
                               "<a href='https://github.com/stepersjmj-hash/capture'>github.com/stepersjmj-hash/capture</a>")
                           .arg(APP_VERSION, keyText(m_hkRegion), keyText(m_hkFull), Defaults::osCaptureKeyText()));
}
