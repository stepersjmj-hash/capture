#pragma once

// 단축키 표기 도우미 — 코드에는 이식 표기("Ctrl+Z")로 적고, 화면(툴팁·메뉴·F1 치트시트)에는
// 플랫폼 표기로 보여 준다: Windows "Ctrl+Z", macOS "⌘Z" (Qt 가 mac 에서 Ctrl 을 ⌘ 로 다룬다).
#include <QKeySequence>
#include <QString>

namespace Keys {

inline QString label(const char *portable) {
    return QKeySequence(QString::fromLatin1(portable), QKeySequence::PortableText).toString(QKeySequence::NativeText);
}

// 삭제 키 — mac 키보드는 ⌫(delete) 가 지우기 키
inline QString del() {
#ifdef Q_OS_MACOS
    return QStringLiteral("⌫");
#else
    return QStringLiteral("Del");
#endif
}

} // namespace Keys
