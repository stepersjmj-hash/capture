// main.cpp — 진입점: 단일 인스턴스, 설정 파일, 스타일, 아이콘 내보내기 도구.
#include "App.h"
#include "Theme.h"

#include <QApplication>
#include <QBuffer>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QIcon>
#include <QImage>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPainter>
#include <QSettings>
#include <QStandardPaths>
#include <QSvgRenderer>

namespace {

const char *kInstanceName = "McaptureSingleInstance";

// SVG 마스터에서 멀티 사이즈 .ico 생성 (PNG 엔트리) — 사용: Mcapture --export-ico <입력.svg> <출력.ico>
bool exportIco(const QString &svgPath, const QString &outPath) {
    QSvgRenderer renderer(svgPath);
    if (!renderer.isValid())
        return false;
    const int sizes[] = {16, 24, 32, 48, 64, 128, 256};
    QList<QByteArray> pngs;
    for (int size : sizes) {
        QImage img(size, size, QImage::Format_ARGB32);
        img.fill(Qt::transparent);
        QPainter p(&img);
        renderer.render(&p);
        p.end();
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        img.save(&buffer, "PNG");
        pngs.append(bytes);
    }
    QFile file(outPath);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    QDataStream ds(&file);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds << quint16(0) << quint16(1) << quint16(std::size(sizes));
    quint32 offset = 6 + 16 * quint32(std::size(sizes));
    for (size_t i = 0; i < std::size(sizes); ++i) {
        const quint8 dim = sizes[i] == 256 ? 0 : quint8(sizes[i]);
        ds << dim << dim << quint8(0) << quint8(0) << quint16(1) << quint16(32) << quint32(pngs[i].size()) << offset;
        offset += pngs[i].size();
    }
    for (const QByteArray &png : pngs)
        file.write(png);
    return true;
}

// SVG 마스터에서 macOS .icns 생성 (PNG 엔트리, iconutil 불필요) — 사용: Mcapture --export-icns <입력.svg> <출력.icns>
bool exportIcns(const QString &svgPath, const QString &outPath) {
    QSvgRenderer renderer(svgPath);
    if (!renderer.isValid())
        return false;
    struct Entry { const char *type; int size; };
    const Entry entries[] = {{"icp4", 16},  {"icp5", 32},  {"icp6", 64},   {"ic07", 128},
                             {"ic08", 256}, {"ic09", 512}, {"ic10", 1024}};
    QList<QByteArray> pngs;
    for (const Entry &e : entries) {
        QImage img(e.size, e.size, QImage::Format_ARGB32);
        img.fill(Qt::transparent);
        QPainter p(&img);
        renderer.render(&p);
        p.end();
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        img.save(&buffer, "PNG");
        pngs.append(bytes);
    }
    quint32 total = 8;
    for (const QByteArray &png : pngs)
        total += 8 + quint32(png.size());
    QFile file(outPath);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    QDataStream ds(&file);
    ds.setByteOrder(QDataStream::BigEndian);
    file.write("icns", 4);
    ds << total;
    for (size_t i = 0; i < std::size(entries); ++i) {
        file.write(entries[i].type, 4);
        ds << quint32(8 + pngs[i].size());
        file.write(pngs[i]);
    }
    return true;
}

// 설정 파일: Windows 는 실행 파일 옆 Mcapture.ini (포터블), macOS 는 Application Support
QString settingsPath() {
#ifdef Q_OS_MACOS
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    return dir + "/Mcapture.ini";
#else
    return QCoreApplication::applicationDirPath() + "/Mcapture.ini";
#endif
}

} // namespace

int main(int argc, char *argv[]) {
    // 캡처 좌표가 모니터별 배율과 정확히 맞도록 (반올림 없이)
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);
    app.setApplicationName("Mcapture");
    app.setApplicationVersion(APP_VERSION);
    app.setQuitOnLastWindowClosed(false);   // 편집 창을 닫아도 트레이에 남는다

    if (argc == 4 && QString::fromLocal8Bit(argv[1]) == "--export-ico")
        return exportIco(QString::fromLocal8Bit(argv[2]), QString::fromLocal8Bit(argv[3])) ? 0 : 1;
    if (argc == 4 && QString::fromLocal8Bit(argv[1]) == "--export-icns")
        return exportIcns(QString::fromLocal8Bit(argv[2]), QString::fromLocal8Bit(argv[3])) ? 0 : 1;

    QStringList args;
    for (int i = 1; i < argc; ++i)
        args.append(QString::fromLocal8Bit(argv[i]));

    // 단일 인스턴스: 이미 떠 있으면 인자만 넘기고 종료 (--region 등으로 외부에서 캡처를 시킬 수 있다)
    {
        QLocalSocket probe;
        probe.connectToServer(kInstanceName);
        if (probe.waitForConnected(300)) {
            probe.write(args.join('\n').toUtf8());
            probe.flush();
            probe.waitForBytesWritten(500);
            return 0;
        }
    }
    QLocalServer::removeServer(kInstanceName);
    auto *server = new QLocalServer(&app);
    server->listen(kInstanceName);

    app.setWindowIcon(QIcon(":/icons/app.svg"));
    QFontDatabase::addApplicationFont(":/fonts/icons.otf");
    QFont font = app.font();
    font.setFamilies({"Pretendard", "Segoe UI", "Malgun Gothic", "Apple SD Gothic Neo"});
    app.setFont(font);
    app.setStyle("Fusion");
    app.setStyleSheet(Theme::appStyleSheet());

    QSettings settings(settingsPath(), QSettings::IniFormat);
    App core(settings);

    QObject::connect(server, &QLocalServer::newConnection, &core, [&core, server] {
        QLocalSocket *client = server->nextPendingConnection();
        if (!client)
            return;
        client->waitForReadyRead(500);
        const QStringList a = QString::fromUtf8(client->readAll()).split('\n', Qt::SkipEmptyParts);
        client->deleteLater();
        core.handleArgs(a, true);
    });

    core.handleArgs(args, false);
    return app.exec();
}
