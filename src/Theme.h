#pragma once

#include <QColor>
#include <QString>

// 디자인 토큰 — Mview 디자인 명세(따뜻한 차콜 + 앰버 포인트)를 그대로 따른다.
// QSS 는 소수 알파를 지원하지 않으므로 0~255 정수로 환산해 표기.
namespace Theme {

inline QColor accent() { return QColor(0xe2, 0xb4, 0x64); }      // #e2b464 앰버
inline QColor accentHover() { return QColor(0xf0, 0xc9, 0x88); }
inline QColor stage() { return QColor(0x0c, 0x0c, 0x0f); }       // 캔버스 배경
inline QColor panel() { return QColor(0x1b, 0x1b, 0x21); }
inline QColor text1() { return QColor(0xec, 0xed, 0xf0); }
inline QColor text2() { return QColor(0x92, 0x96, 0xa1); }

inline QString appStyleSheet() {
    return QStringLiteral(R"(
QWidget { color: #dfe1e6; font-size: 13px; }
QMainWindow, QDialog, QMessageBox, QWidget#editorRoot { background: #141418; }
QWidget#toolRow { background: #1a1a20; border-bottom: 1px solid rgba(255,255,255,20); }
QWidget#canvas { background: #0c0c0f; }

QToolButton[cls="tool"] {
    background: transparent; border: none; border-radius: 10px;
    padding: 4px 6px; color: #c7cad2; font-size: 11px;
    min-width: 46px;
}
QToolButton[cls="tool"]:hover { background: rgba(255,255,255,20); }
QToolButton[cls="tool"]:pressed { background: rgba(255,255,255,36); }
QToolButton[cls="tool"]:checked { background: rgba(226,180,100,38); color: #f0c988; }
QToolButton[cls="tool"]:disabled { color: #4a4d57; }
QToolButton::menu-indicator { image: none; }

QToolButton[cls="chip"] {
    border: 2px solid rgba(255,255,255,40); border-radius: 11px;
    min-width: 18px; max-width: 18px; min-height: 18px; max-height: 18px; padding: 0;
}
QToolButton[cls="chip"]:hover { border-color: rgba(255,255,255,120); }
QToolButton[cls="chip"]:checked { border-color: #e2b464; }

QFrame#sep { background: rgba(255,255,255,26); max-width: 1px; margin: 6px 4px; }
QLabel#dim { color: #9296a1; font-size: 12px; }
QLabel#status { color: #9296a1; font-size: 12px; }
QLabel#statusOk { color: #e2b464; font-size: 12px; }
QStatusBar { background: #1a1a20; border-top: 1px solid rgba(255,255,255,20); }
QStatusBar::item { border: none; }

QSpinBox, QLineEdit, QKeySequenceEdit, QComboBox {
    background: #111116; border: 1px solid rgba(255,255,255,30); border-radius: 8px;
    padding: 4px 8px; color: #ecedf0; selection-background-color: #e2b464; selection-color: #141418;
}
QSpinBox:focus, QLineEdit:focus, QKeySequenceEdit:focus { border-color: #e2b464; }
QSpinBox::up-button, QSpinBox::down-button { width: 14px; border: none; background: transparent; }
QSpinBox::up-arrow { image: none; width: 0; height: 0; border-left: 4px solid transparent; border-right: 4px solid transparent; border-bottom: 5px solid #9296a1; }
QSpinBox::down-arrow { image: none; width: 0; height: 0; border-left: 4px solid transparent; border-right: 4px solid transparent; border-top: 5px solid #9296a1; }
QKeySequenceEdit QLineEdit { border: none; background: transparent; padding: 0; }

QPushButton {
    background: #22222a; border: 1px solid rgba(255,255,255,30); border-radius: 9px;
    padding: 6px 16px; color: #ecedf0; font-size: 13px;
}
QPushButton:hover { background: #2a2a33; }
QPushButton:pressed { background: #1a1a20; }
QPushButton:default { background: #e2b464; color: #141418; border-color: #e2b464; font-weight: 600; }
QPushButton:default:hover { background: #f0c988; }
QPushButton:disabled { color: #6b6f7a; }

QCheckBox { spacing: 8px; }
QCheckBox::indicator { width: 16px; height: 16px; border-radius: 4px; border: 1px solid rgba(255,255,255,60); background: #111116; }
QCheckBox::indicator:checked { background: #e2b464; border-color: #e2b464; }

QGroupBox { border: 1px solid rgba(255,255,255,26); border-radius: 12px; margin-top: 12px; padding: 12px 10px 6px 10px; }
QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; color: #9296a1; }

QMenu { background: #16161b; border: 1px solid rgba(255,255,255,26); border-radius: 10px; padding: 6px; }
QMenu::item { padding: 6px 28px 6px 14px; border-radius: 6px; color: #dfe1e6; }
QMenu::item:selected { background: rgba(255,255,255,20); }
QMenu::item:disabled { color: #6b6f7a; }
QMenu::separator { height: 1px; background: rgba(255,255,255,20); margin: 6px 8px; }
QMenu::indicator { width: 14px; height: 14px; margin-left: 4px; }
QMenu::indicator:checked { image: none; background: #e2b464; border-radius: 3px; }
QMenu::indicator:unchecked { image: none; background: transparent; border: 1px solid rgba(255,255,255,50); border-radius: 3px; }

QToolTip { background: #22222a; color: #ecedf0; border: 1px solid rgba(255,255,255,40); padding: 4px 8px; }
QScrollBar:vertical { background: transparent; width: 10px; }
QScrollBar::handle:vertical { background: rgba(255,255,255,50); border-radius: 5px; min-height: 24px; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; }
)");
}

} // namespace Theme
