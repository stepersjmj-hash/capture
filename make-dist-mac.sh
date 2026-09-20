#!/bin/bash
# macOS 배포 스크립트 — Mac(또는 CI macos 러너)에서 실행 (make-dist.ps1 의 맥 대응)
# 산출물: dist/Mcapture-<버전>-mac.dmg
#
# 사전 준비 (Homebrew):  brew install cmake ninja qt
set -euo pipefail
cd "$(dirname "$0")"

VERSION=$(sed -n 's/^project(Mcapture VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
# 개발 빌드와 분리된 전용 디렉토리 — macdeployqt 가 번들에 심은 Qt 와 개발 중 재링크된
# 바이너리가 섞이면 시작 시 abort 가 나므로 공유하지 않는다
BUILD=build-mac-dist
APP=$BUILD/Mcapture.app

echo "== Mcapture v$VERSION macOS 빌드 =="

cmake -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" -j

# 앱 아이콘이 없으면 SVG 마스터에서 생성 후 재빌드 (번들에 포함시키기 위함)
if [ ! -f assets/app.icns ]; then
    "$APP/Contents/MacOS/Mcapture" --export-icns assets/app.svg assets/app.icns
    cmake -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build "$BUILD" -j
fi

# Qt 와 의존 dylib 을 번들 Frameworks 로 복사
"$(brew --prefix qt)/bin/macdeployqt" "$APP" -always-overwrite

# rpath 교정: /opt/homebrew/lib 을 지우고 번들 Frameworks 로 — Homebrew 없는 맥에서도 실행되게
install_name_tool -delete_rpath /opt/homebrew/lib "$APP/Contents/MacOS/Mcapture" 2>/dev/null || true
install_name_tool -add_rpath "@executable_path/../Frameworks" "$APP/Contents/MacOS/Mcapture" 2>/dev/null || true

strip -x "$APP/Contents/MacOS/Mcapture" || true

# Apple Silicon 은 서명 없는 바이너리를 실행하지 않으므로 ad-hoc 서명 (macdeployqt 가 서명을 깨뜨림)
codesign --force --deep -s - "$APP"

mkdir -p dist
DMG="dist/Mcapture-$VERSION-mac.dmg"
rm -f "$DMG"
STAGE=$(mktemp -d)
cp -R "$APP" "$STAGE/"
ln -s /Applications "$STAGE/Applications"
hdiutil create -volname "Mcapture" -srcfolder "$STAGE" -ov -format UDZO "$DMG"
rm -rf "$STAGE"

echo "== 완료: $DMG =="
echo "첫 실행 시 Gatekeeper 경고가 나오면: 앱 우클릭 → 열기. 첫 캡처 때 '화면 기록' 권한을 허용하고 앱을 다시 실행할 것"
