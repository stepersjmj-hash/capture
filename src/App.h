#pragma once

// 앱 본체 — 트레이 아이콘·메뉴, 전역 단축키, 클립보드 감시, 편집 창 띄우기, 자동 업데이트.
// 창이 하나도 없어도 트레이에 상주한다 (main.cpp 가 setQuitOnLastWindowClosed(false)).
#include <QElapsedTimer>
#include <QImage>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QTimer>

class ClipboardWatch;
class EditorWindow;
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
    void onClipboardImage(const QImage &img);   // 스니핑 도구의 전체 화면 선행 복사 처리
    bool isScreenSized(const QSize &size) const;
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

    // 클립보드 캡처 정리: Win+Shift+S 는 시작 때 전체 화면을, 선택 뒤 영역을 잇달아 클립보드에 넣는다.
    // 전체 화면 크기 이미지는 잠시 보류(m_pendingClip)했다가 다음 이미지가 오면 버리고,
    // 직전에 클립보드로 연 창이 편집 전이면 새 창 대신 그 창의 이미지를 바꾼다.
    QImage m_pendingClip;
    QTimer m_pendingTimer;
    QPointer<EditorWindow> m_lastClipWin;
    QElapsedTimer m_lastClipTime;
};
