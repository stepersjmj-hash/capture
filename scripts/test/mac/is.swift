// is.swift — 키보드 입력 소스 조회/변경 (검증용). 빌드: swiftc -o is is.swift
// 사용: ./is get | ./is set com.apple.keylayout.ABC | ./is set com.apple.inputmethod.Korean.2SetKorean
import Carbon
let args = CommandLine.arguments
if args.count > 1 && args[1] == "set" {
    let list = TISCreateInputSourceList(nil, false).takeRetainedValue() as! [TISInputSource]
    for src in list {
        let id = Unmanaged<CFString>.fromOpaque(TISGetInputSourceProperty(src, kTISPropertyInputSourceID)).takeUnretainedValue() as String
        if id == args[2] { print("select ->", TISSelectInputSource(src)); exit(0) }
    }
    print("not found"); exit(1)
}
let cur = TISCopyCurrentKeyboardInputSource().takeRetainedValue()
print(Unmanaged<CFString>.fromOpaque(TISGetInputSourceProperty(cur, kTISPropertyInputSourceID)).takeUnretainedValue() as String)
