#pragma once

// 앱 본체 — 트레이 아이콘·메뉴, 전역 단축키, 클립보드 감시, 편집 창 띄우기, 자동 업데이트.
// 창이 하나도 없어도 트레이에 상주한다 (main.cpp 가 setQuitOnLastWindowClosed(false)).
#include <QImage>
#include <QObject>
#include <QStringList>

class ClipboardWatch;
class HotKey;
class QAction;
class QMenu;
class QSettings;
class QSystemTrayIcon;
class Updater;

class App : public QObject {
    Q_OBJECT
public:
    explicit App(QSettings &settings, QObject *parent = nullptr);

    // 명령줄 / 다른 인스턴스가 보낸 인자 처리: --region --full --edit <파일> --settings --quit, 이미지 경로
    void handleArgs(const QStringList &args, bool fromOtherInstance);

public slots:
    void captureRegion(int delayMs = 0);
    void captureFull(int delayMs = 0);
    void openClipboardImage();
    void showSettings();

private:
    void buildTray();
    void refreshMenu();
    void applySettings(bool interactive);
    void openEditor(const QImage &img, bool fromCapture);
    void setupUpdater();
    void showAbout();
    QString keyText(const HotKey *hk) const;

    QSettings &m_settings;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_menu = nullptr;
    QAction *m_actRegion = nullptr;
    QAction *m_actFull = nullptr;
    QAction *m_actWatch = nullptr;
    QAction *m_actAutoUpdate = nullptr;
    QAction *m_actUpdateNow = nullptr;
    HotKey *m_hkRegion = nullptr;
    HotKey *m_hkFull = nullptr;
    ClipboardWatch *m_watch = nullptr;
    Updater *m_updater = nullptr;
    bool m_updateClickPending = false;
};
