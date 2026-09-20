#pragma once

// 간단한 진단 로그 — %TEMP%/Mcapture.log 에 시각(ms)과 함께 한 줄씩 덧붙인다.
// GUI 앱이라 콘솔이 없어 "왜 창이 두 개 떴나" 같은 흐름 추적용. 개인 데이터는 쓰지 않는다(크기·이벤트만).
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QString>

inline void mlog(const QString &msg) {
    QFile f(QDir::tempPath() + "/Mcapture.log");
    if (f.size() > 512 * 1024)   // 너무 커지면 새로 시작
        f.remove();
    if (f.open(QIODevice::Append | QIODevice::Text))
        f.write((QDateTime::currentDateTime().toString("HH:mm:ss.zzz ") + msg + "\n").toUtf8());
}
