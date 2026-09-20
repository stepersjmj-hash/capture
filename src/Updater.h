// Updater.h — NAS 자동 업데이트 (Mplayer 에서 이식)
// 접속 대상은 오직 내 NAS(stepersjmj.synology.me — HTTPS GET)뿐이며 보내는 데이터는 없다.
// 릴리즈 스크립트가 \\mjj\web\site\mjimage\MJ_data\Mplayer 에 올리는 매니페스트
// (2줄: 버전 / zip 파일명)와 zip 을 읽는다. 플랫폼별 매니페스트·페이로드 —
// Windows: version.txt + dist zip(배치 스왑, release.ps1) /
// macOS: version-mac.txt + .app zip(bash 스왑, release-mac.sh).
#pragma once
#include <QObject>
#include <QString>

class QSettings;
class QNetworkAccessManager;

class Updater : public QObject {
    Q_OBJECT
public:
    explicit Updater(QSettings *settings, QObject *parent = nullptr);

    bool autoEnabled() const;              // [update] auto (기본 켜짐)
    void setAuto(bool on);
    QString foundVersion() const { return m_foundVersion; }

    void check(bool manual);               // version.txt 확인 (비동기)
    void downloadAndInstall();             // zip 다운로드 → 풀기 → 교체 배치 → installReady

signals:
    void updateFound(const QString &version, bool manual);  // 새 버전 발견
    void upToDate(const QString &current);                  // 수동 확인: 이미 최신
    void checkFailed();                                     // 수동 확인: NAS 연결 실패
    void installReady();     // 교체 준비 완료 — 받은 쪽에서 앱을 종료하면 배치가 교체·재시작
    void installFailed(const QString &error);

private:
    QString baseUrl() const;               // [update] url= 로 재지정 가능
    static bool isNewer(const QString &v); // APP_VERSION 과 비교
    void extractAndStage(const QString &zipPath);
    void launchSwap(const QString &payloadDir);

    QSettings *m_settings;
    QNetworkAccessManager *m_nam;
    QString m_foundVersion, m_zipName;
    bool m_busy = false;
};
