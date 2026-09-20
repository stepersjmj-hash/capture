// Updater.cpp — NAS 자동 업데이트 구현 (Qt Network — 이미 링크되어 있음)
#include "Updater.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QTime>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

static const char *kUpdBaseDefault =
    "https://stepersjmj.synology.me:28443/mjimage/MJ_data/Mcapture";

// 플랫폼별 매니페스트 — 페이로드가 다르다 (Win: dist zip / mac: .app zip).
// 릴리스 스크립트: Windows release.ps1 → version.txt, Mac release-mac.sh → version-mac.txt
#ifdef Q_OS_MACOS
static const char *kManifestName = "version-mac.txt";
#else
static const char *kManifestName = "version.txt";
#endif

// 진행 로그 (%TEMP%/McaptureUpd/upd.log) — GUI 앱이라 콘솔이 없어 문제 추적용
static void updLog(const QString &msg) {
    QFile f(QDir::tempPath() + "/McaptureUpd/upd.log");
    if (f.open(QIODevice::Append)) {
        f.write((QTime::currentTime().toString("HH:mm:ss ") + msg + "\n").toUtf8());
        f.close();
    }
}

Updater::Updater(QSettings *settings, QObject *parent)
    : QObject(parent), m_settings(settings),
      m_nam(new QNetworkAccessManager(this)) {}

bool Updater::autoEnabled() const {
    return m_settings->value("update/auto", true).toBool();
}

void Updater::setAuto(bool on) { m_settings->setValue("update/auto", on); }

QString Updater::baseUrl() const {
    QString u = m_settings->value("update/url", kUpdBaseDefault).toString().trimmed();
    while (u.endsWith('/'))
        u.chop(1);
    return u.isEmpty() ? kUpdBaseDefault : u;
}

// "1.3.4" 형식 비교 — 현재 버전은 APP_VERSION (CMake project VERSION,
// 타깃 전체 컴파일 정의라 버전이 바뀌면 전 소스가 재컴파일된다 = 스테일 없음)
bool Updater::isNewer(const QString &v) {
    const QStringList a = v.split('.'), b = QString(APP_VERSION).split('.');
    for (int i = 0; i < 3; ++i) {
        const int x = a.value(i).toInt(), y = b.value(i).toInt();
        if (x != y)
            return x > y;
    }
    return false;
}

void Updater::check(bool manual) {
    if (m_busy)
        return;
    QNetworkRequest req(QUrl(baseUrl() + "/" + kManifestName));
    req.setTransferTimeout(10000);
    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, manual] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            if (manual)
                emit checkFailed();   // 자동 확인은 조용히 실패
            return;
        }
        const QStringList lines =
            QString::fromUtf8(reply->readAll()).split('\n', Qt::SkipEmptyParts);
        const QString ver = lines.value(0).trimmed();
        const QString zip = lines.value(1).trimmed();
        if (ver.isEmpty() || zip.isEmpty()) {
            if (manual)
                emit checkFailed();
            return;
        }
        if (isNewer(ver)) {
            m_foundVersion = ver;
            m_zipName = zip;
            emit updateFound(ver, manual);
        } else if (manual) {
            emit upToDate(APP_VERSION);
        }
    });
}

void Updater::downloadAndInstall() {
    if (m_busy || m_zipName.isEmpty())
        return;
    m_busy = true;
    const QString dir = QDir::tempPath() + "/McaptureUpd";
    QDir().mkpath(dir);
    const QString zipPath = dir + "/" + m_zipName;
    QNetworkRequest req(QUrl(baseUrl() + "/" + m_zipName));
    req.setTransferTimeout(300000);   // dist zip은 수십 MB — 넉넉히
    QNetworkReply *reply = m_nam->get(req);
    auto *out = new QFile(zipPath, reply);
    if (!out->open(QIODevice::WriteOnly)) {
        reply->abort();
        reply->deleteLater();
        m_busy = false;
        emit installFailed("임시 파일을 만들지 못했습니다.");
        return;
    }
    connect(reply, &QNetworkReply::readyRead, this,
            [reply, out] { out->write(reply->readAll()); });
    connect(reply, &QNetworkReply::finished, this, [this, reply, out, zipPath] {
        out->write(reply->readAll());
        out->close();
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            QFile::remove(zipPath);
            m_busy = false;
            emit installFailed("NAS에서 업데이트 파일을 받지 못했습니다.");
            return;
        }
        updLog("download ok: " + zipPath);
        extractAndStage(zipPath);
    });
}

// 내장 tar(bsdtar — Win10 1803+/macOS 기본)로 zip을 푼다. 앱엔 압축 엔진이 없다.
void Updater::extractAndStage(const QString &zipPath) {
    const QString xdir = QFileInfo(zipPath).absolutePath() + "/x";
    QDir(xdir).removeRecursively();
    QDir().mkpath(xdir);
    auto *tar = new QProcess(this);
    connect(tar, &QProcess::finished, this,
            [this, tar, xdir](int code, QProcess::ExitStatus st) {
                updLog(QString("tar finished: code=%1 st=%2").arg(code).arg(int(st)));
                tar->deleteLater();
#ifdef Q_OS_MACOS
                // mac 페이로드: ditto zip 루트 = Mcapture.app 번들
                const QString payload = xdir + "/Mcapture.app";
                const QString probe = payload + "/Contents/MacOS/Mcapture";
#else
                const QString payload = xdir + "/Mcapture";   // zip 루트 = dist\Mplayer 폴더
                const QString probe = payload + "/Mcapture.exe";
#endif
                if (st != QProcess::NormalExit || code != 0 ||
                    !QFileInfo::exists(probe)) {
                    m_busy = false;
                    emit installFailed("업데이트 파일을 풀지 못했습니다.");
                    return;
                }
                launchSwap(payload);
            });
    updLog("tar starting");
    tar->start("tar", {"-xf", QDir::toNativeSeparators(zipPath), "-C",
                       QDir::toNativeSeparators(xdir)});
    if (!tar->waitForStarted(5000)) {
        tar->deleteLater();
        m_busy = false;
        emit installFailed("압축 해제 도구(tar)를 실행하지 못했습니다.");
    }
}

void Updater::launchSwap(const QString &payloadDir) {
#ifdef Q_OS_WIN
    const QString appDir =
        QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
    const QString exePath = appDir + "\\Mcapture.exe";
    const QString payload = QDir::toNativeSeparators(payloadDir);
    const QString bat = QDir::toNativeSeparators(QDir::tempPath()) + "\\McaptureUpd\\swap.cmd";
    // 교체 배치: 종료 대기 → dist 전체(exe+DLL) 덮어쓰기 → 재실행 → 자기 삭제.
    // cmd는 시스템 로캘(CP949)로 배치를 읽으므로 949로 변환해 한글 경로를 지킨다.
    const QString wt =
        "@echo off\r\n"
        ":wait\r\n"
        "ping -n 2 127.0.0.1 >nul\r\n"
        "del \"" + exePath + "\" >nul 2>&1\r\n"
        "if exist \"" + exePath + "\" goto wait\r\n"
        "xcopy /e /y /q \"" + payload + "\\*\" \"" + appDir + "\\\" >nul\r\n"
        "start \"\" \"" + exePath + "\"\r\n"
        "del \"%~f0\"\r\n";
    std::wstring ws = wt.toStdWString();
    int n = WideCharToMultiByte(949, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string mb(n, '\0');
    WideCharToMultiByte(949, 0, ws.c_str(), -1, &mb[0], n, nullptr, nullptr);
    while (!mb.empty() && mb.back() == '\0')
        mb.pop_back();
    QFile f(bat);
    if (!f.open(QIODevice::WriteOnly)) {
        m_busy = false;
        emit installFailed("교체 스크립트를 만들지 못했습니다.");
        return;
    }
    f.write(mb.data(), qint64(mb.size()));
    f.close();
    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    std::wstring wbat = QDir::toNativeSeparators(bat).toStdWString();
    sei.lpFile = wbat.c_str();
    sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei)) {
        m_busy = false;
        emit installFailed("교체 스크립트를 실행하지 못했습니다.");
        return;
    }
    updLog("swap launched");
    emit installReady();   // 받은 쪽에서 앱을 종료하면 배치가 교체·재시작한다
#elif defined(Q_OS_MACOS)
    // 번들 통째 교체: 종료 대기 → rm -rf → ditto 복사 → quarantine 제거 → 재실행.
    // ditto 는 cp -R 과 달리 서명·확장 속성을 보존한다 (ad-hoc 서명 유지에 필요).
    const QString bundle =
        QDir(QCoreApplication::applicationDirPath() + "/../..").canonicalPath();
    if (!bundle.endsWith(".app")) {   // 번들 밖(개발 빌드)에서는 교체하지 않는다
        m_busy = false;
        emit installFailed("앱 번들 밖에서는 자동 교체할 수 없습니다.");
        return;
    }
    const QString sh = QDir::tempPath() + "/McaptureUpd/swap.sh";
    const QString script =
        "#!/bin/bash\n"
        "while kill -0 " + QString::number(QCoreApplication::applicationPid()) +
        " 2>/dev/null; do sleep 0.5; done\n"
        "rm -rf \"" + bundle + "\"\n"
        "ditto \"" + payloadDir + "\" \"" + bundle + "\"\n"
        "xattr -dr com.apple.quarantine \"" + bundle + "\" 2>/dev/null\n"
        "open -n \"" + bundle + "\"\n"
        "rm -f \"$0\"\n";
    QFile f(sh);
    if (!f.open(QIODevice::WriteOnly)) {
        m_busy = false;
        emit installFailed("교체 스크립트를 만들지 못했습니다.");
        return;
    }
    f.write(script.toUtf8());
    f.close();
    f.setPermissions(f.permissions() | QFileDevice::ExeOwner);
    if (!QProcess::startDetached("/bin/bash", {sh})) {
        m_busy = false;
        emit installFailed("교체 스크립트를 실행하지 못했습니다.");
        return;
    }
    updLog("swap launched (mac)");
    emit installReady();
#else
    Q_UNUSED(payloadDir);
#endif
}
