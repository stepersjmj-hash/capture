# 배포 패키지 생성: build/ 에서 실행에 필요한 파일만 추려 dist/Mcapture 를 만들고 zip 으로 압축
# 사용: pwsh -File make-dist.ps1   (먼저 빌드 + DLL 배포가 되어 있어야 한다 — CLAUDE.md "실행" 참고)
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$build = Join-Path $root "build"
$dist = Join-Path $root "dist\Mcapture"
$zip = Join-Path $root "dist\Mcapture.zip"

if (-not (Test-Path (Join-Path $build "Mcapture.exe"))) {
    Write-Error "build\Mcapture.exe 가 없습니다. 먼저 빌드하세요."
}

Remove-Item -Recurse -Force (Join-Path $root "dist") -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $dist | Out-Null

# 실행 파일 + DLL (Mcapture.ini 는 개인 설정이므로 제외)
Copy-Item (Join-Path $build "Mcapture.exe") $dist
Copy-Item (Join-Path $build "*.dll") $dist

# Qt 플러그인 (있는 것만). tls 는 windeployqt 가 빼놓지만 자동 업데이트(HTTPS)에 필수
foreach ($plugin in "platforms", "styles", "iconengines", "imageformats", "tls") {
    $p = Join-Path $build $plugin
    if (Test-Path $p) {
        $dest = Join-Path $dist $plugin
        Remove-Item -Recurse -Force $dest -ErrorAction SilentlyContinue
        Copy-Item -Recurse $p $dest
    }
}
if (-not (Test-Path (Join-Path $dist "tls\qschannelbackend.dll"))) {
    Write-Error "tls\qschannelbackend.dll 이 없습니다 — 자동 업데이트가 조용히 실패합니다 (CLAUDE.md 참고)"
}

Compress-Archive -Path $dist -DestinationPath $zip -CompressionLevel Optimal -Force

$size = (Get-ChildItem $dist -Recurse -File | Measure-Object Length -Sum).Sum / 1MB
$zipSize = (Get-Item $zip).Length / 1MB
"완료: dist\Mcapture ({0:N0} MB) → dist\Mcapture.zip ({1:N0} MB)" -f $size, $zipSize
