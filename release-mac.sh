#!/bin/bash
# macOS 릴리즈 — Mac에서 실행 (release.ps1의 맥 대응)
# make-dist-mac.sh 로 빌드된 번들에서 dmg(수동 배포용) + .app zip(자동 업데이트 페이로드)을
# 만들고, GitHub Release(기존 v<버전> 태그에 업로드) + NAS(version-mac.txt) 에 올린다.
#
# NAS 는 Finder 에서 smb://mjj 접속 후 web 볼륨 마운트 (→ /Volumes/web)
set -euo pipefail
cd "$(dirname "$0")"

VERSION=$(sed -n 's/^project(Mcapture VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
APP=build-mac-dist/Mcapture.app
DMG=dist/Mcapture-$VERSION-mac.dmg
ZIP=dist/Mcapture-$VERSION-mac.zip
NAS=/Volumes/web/site/mjimage/MJ_data/Mcapture   # = https://stepersjmj.synology.me:28443/mjimage/MJ_data/Mcapture/

[ -d "$APP" ] || { echo "먼저 ./make-dist-mac.sh 로 빌드하세요"; exit 1; }
[ -f "$DMG" ] || { echo "$DMG 가 없습니다 — make-dist-mac.sh 산출물 확인"; exit 1; }
[ -d "$NAS" ] || { echo "NAS 미마운트: Finder 에서 smb://mjj → web 연결 후 재실행"; exit 1; }

# .app zip — ditto 가 서명·확장 속성을 보존한다 (Updater 가 이 zip 을 tar 로 푼다)
rm -f "$ZIP"
ditto -c -k --keepParent "$APP" "$ZIP"

# GitHub Release: Windows release.ps1 이 만든 v<버전> 릴리스에 mac 파일 추가
gh release upload "v$VERSION" "$DMG" "$ZIP" --clobber

# NAS 업로드 — mac 앱의 자동 업데이트가 이 폴더의 version-mac.txt 를 읽는다
cp -f "$ZIP" "$NAS/"
printf '%s\r\n%s\r\n' "$VERSION" "$(basename "$ZIP")" > "$NAS/version-mac.txt"
echo "== 릴리즈 완료: v$VERSION → GitHub(dmg+zip) + $NAS =="
