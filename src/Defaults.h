#pragma once

// 설정 기본값 — App(트레이·단축키), SettingsDialog, EditorWindow 가 함께 쓴다.
#include <QKeySequence>
#include <QStandardPaths>
#include <QString>

namespace Defaults {

// 영역 캡처. macOS 는 Qt 가 Ctrl↔Cmd 를 맞바꾸므로 "Meta+Shift+Ctrl+S" = ⌃⇧⌘S.
inline QKeySequence regionKey() {
#ifdef Q_OS_MACOS
    return QKeySequence(QStringLiteral("Meta+Shift+Ctrl+S"));
#else
    return QKeySequence(QStringLiteral("Ctrl+Shift+Alt+S"));
#endif
}

inline QKeySequence fullKey() {
#ifdef Q_OS_MACOS
    return QKeySequence(QStringLiteral("Meta+Shift+Ctrl+F"));
#else
    return QKeySequence(QStringLiteral("Ctrl+Shift+Alt+F"));
#endif
}

inline QString saveDir() {
    return QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + QStringLiteral("/Mcapture");
}

// OS 기본 캡처 단축키 표기 (안내문용)
inline QString osCaptureKeyText() {
#ifdef Q_OS_MACOS
    return QStringLiteral("⌃⇧⌘4");
#else
    return QStringLiteral("Win+Shift+S");
#endif
}

} // namespace Defaults
