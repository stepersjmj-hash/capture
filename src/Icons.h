#pragma once

// Material Icons Outlined(동봉 폰트, assets/fonts) 글리프를 QIcon 으로 만든다.
// 툴바 버튼이 "아이콘 + 한글 라벨" 을 함께 보여야 해서 폰트 텍스트가 아닌 QIcon 이 필요하다.
#include <QColor>
#include <QFont>
#include <QIcon>
#include <QPainter>
#include <QPixmap>

namespace Icons {

// 코드포인트 (assets/fonts/MaterialIconsOutlined-Regular.codepoints 참고)
constexpr char16_t kSelect = 0xe569;      // near_me
constexpr char16_t kRect = 0xe835;        // check_box_outline_blank
constexpr char16_t kLine = 0xf108;        // horizontal_rule
constexpr char16_t kArrow = 0xe941;       // arrow_right_alt
constexpr char16_t kText = 0xe264;        // title
constexpr char16_t kFill = 0xe23a;        // format_color_fill
constexpr char16_t kUndo = 0xe166;
constexpr char16_t kRedo = 0xe15a;
constexpr char16_t kDelete = 0xe872;
constexpr char16_t kCopy = 0xf08a;        // content_copy
constexpr char16_t kSave = 0xe161;
constexpr char16_t kSaveAs = 0xeb60;
constexpr char16_t kSettings = 0xe8b8;
constexpr char16_t kFolder = 0xe2c8;      // folder_open
constexpr char16_t kPalette = 0xe40a;
constexpr char16_t kCamera = 0xe412;      // photo_camera
constexpr char16_t kCrop = 0xe3c2;        // crop_free
constexpr char16_t kMonitor = 0xec08;     // screenshot_monitor
constexpr char16_t kPaste = 0xf098;       // content_paste
constexpr char16_t kInfo = 0xe88e;
constexpr char16_t kPower = 0xe8ac;

inline QPixmap glyphPixmap(char16_t cp, const QColor &color, int px, int dpr) {
    QPixmap pm(px * dpr, px * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    QFont f("Material Icons Outlined");
    f.setPixelSize(px);
    p.setFont(f);
    p.setPen(color);
    p.drawText(QRect(0, 0, px, px), Qt::AlignCenter, QString(QChar(cp)));
    return pm;
}

// 평소 색(off) / 체크됐을 때 색(on) 두 상태를 가진 아이콘
inline QIcon glyph(char16_t cp, const QColor &off = QColor(0xc7, 0xca, 0xd2),
                   const QColor &on = QColor(0xf0, 0xc9, 0x88), int px = 20) {
    QIcon icon;
    for (int dpr : {1, 2}) {
        icon.addPixmap(glyphPixmap(cp, off, px, dpr), QIcon::Normal, QIcon::Off);
        icon.addPixmap(glyphPixmap(cp, on, px, dpr), QIcon::Normal, QIcon::On);
        icon.addPixmap(glyphPixmap(cp, QColor(0x4a, 0x4d, 0x57), px, dpr), QIcon::Disabled, QIcon::Off);
    }
    return icon;
}

} // namespace Icons
