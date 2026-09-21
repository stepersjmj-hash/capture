// input.swift — CGEvent 로 마우스 드래그·키 입력 (UI 검증용, 손쉬운 접근 권한 필요).
// 빌드: swiftc -o input input.swift   사용: ./input drag x1 y1 x2 y2 | ./input key <keycode> [ctrl] [shift] [cmd]
// 키코드: S=1 F=3 C=8 W=13 T=17 U=32 M=46 .=47 Space=49 Esc=53
import CoreGraphics
import Foundation
let args = CommandLine.arguments
func post(_ e: CGEvent?, _ f: CGEventFlags = []) { e?.flags = f; e?.post(tap: .cghidEventTap); usleep(40_000) }
if args[1] == "drag" {
    let p1 = CGPoint(x: Double(args[2])!, y: Double(args[3])!), p2 = CGPoint(x: Double(args[4])!, y: Double(args[5])!)
    post(CGEvent(mouseEventSource: nil, mouseType: .mouseMoved, mouseCursorPosition: p1, mouseButton: .left)); usleep(200_000)
    let down = CGEvent(mouseEventSource: nil, mouseType: .leftMouseDown, mouseCursorPosition: p1, mouseButton: .left)
    down?.setIntegerValueField(.mouseEventClickState, value: 1); post(down); usleep(150_000)
    let steps = 12
    for i in 1...steps {
        let t = Double(i) / Double(steps)
        let p = CGPoint(x: p1.x + (p2.x - p1.x) * t, y: p1.y + (p2.y - p1.y) * t)
        let m = CGEvent(mouseEventSource: nil, mouseType: .leftMouseDragged, mouseCursorPosition: p, mouseButton: .left)
        m?.setIntegerValueField(.mouseEventClickState, value: 1); post(m)
    }
    usleep(150_000)
    let up = CGEvent(mouseEventSource: nil, mouseType: .leftMouseUp, mouseCursorPosition: p2, mouseButton: .left)
    up?.setIntegerValueField(.mouseEventClickState, value: 1); post(up)
} else if args[1] == "key" {
    let code = CGKeyCode(UInt16(args[2])!)
    var flags: CGEventFlags = []
    if args.contains("ctrl") { flags.insert(.maskControl) }
    if args.contains("shift") { flags.insert(.maskShift) }
    if args.contains("cmd") { flags.insert(.maskCommand) }
    let d = CGEvent(keyboardEventSource: nil, virtualKey: code, keyDown: true); post(d, flags)
    let u = CGEvent(keyboardEventSource: nil, virtualKey: code, keyDown: false); post(u, [])
}
print("ok")
