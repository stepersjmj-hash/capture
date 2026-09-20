# Mcapture 릴리즈(양 플랫폼, 이 PC 에서만): dist zip → git tag + GitHub Release → NAS 업로드(Win)
#   → 태그 CI(build.yml) 완료 대기 → 릴리스의 mac zip 을 받아 NAS 업로드(mac 자동 업데이트)
# 사용: pwsh -File release.ps1 [-SkipMac]   (CMakeLists.txt project VERSION 기준.
#       먼저 빌드 + make-dist.ps1 로 dist\Mcapture.zip 을 만들어 둘 것)
#   -SkipMac : mac NAS 배포 생략 (CI dmg 를 맥에서 먼저 확인하고 싶을 때 — 나중에
#              같은 명령을 다시 돌리면 Win 쪽은 파일만 교체하고 mac 단계를 이어서 한다)
param([switch]$SkipMac)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$cm = Get-Content "$root\CMakeLists.txt" -Raw
if ($cm -notmatch 'project\(Mcapture VERSION ([0-9.]+)') { throw 'CMakeLists.txt에서 버전을 찾지 못했습니다' }
$ver = $Matches[1]
$zipName = "Mcapture-$ver-win.zip"
$nas = "\\mjj\web\site\mjimage\MJ_data\Mcapture"   # = https://stepersjmj.synology.me:28443/mjimage/MJ_data/Mcapture/

# 1) 검증: dist zip 존재 + exe 가 이번 빌드인지 (dist 는 make-dist.ps1 산출물)
$distZip = "$root\dist\Mcapture.zip"
if (-not (Test-Path $distZip)) { throw 'dist\Mcapture.zip 이 없습니다 — make-dist.ps1 먼저' }
$zip = "$root\dist\$zipName"
Copy-Item $distZip $zip -Force

# 2) 태그 + GitHub Release
#    태그 푸시로 GitHub Actions(build.yml)가 돌아 win zip·mac dmg·mac zip 을 같은 릴리스에
#    --clobber 첨부한다. 릴리스가 이미 있으면(CI 가 먼저 만든 경우) 파일만 교체 — 멱등.
Set-Location $root
git tag -a "v$ver" -m "v$ver" 2>$null
git push origin "v$ver"
gh release view "v$ver" *> $null
if ($LASTEXITCODE -eq 0) {
    gh release upload "v$ver" $zip --clobber
} else {
    gh release create "v$ver" $zip --title "Mcapture v$ver" --notes "변경 사항은 CHANGELOG.md 참고."
}

# 3) NAS 업로드 — 앱 자동 업데이트가 이 폴더를 읽는다 (페이로드 = dist zip)
New-Item -ItemType Directory -Force $nas | Out-Null
Copy-Item $zip "$nas\$zipName" -Force
# version.txt: 1줄 = 버전, 2줄 = zip 파일명 (UTF-8, BOM 없이)
[IO.File]::WriteAllText("$nas\version.txt", "$ver`r`n$zipName`r`n", (New-Object System.Text.UTF8Encoding $false))
Write-Host "Windows 릴리즈 완료: v$ver → GitHub + $nas"

# 4) mac — 태그 푸시로 돌아간 GitHub Actions 가 mac dmg·zip 을 릴리스에 첨부하면,
#    그 zip(.app 을 ditto 로 압축 — Updater 가 tar 로 푼다)을 NAS 에 올리고 version-mac.txt 갱신.
#    실기기 검증 없이 나가는 셈이므로 맥 쪽 코드를 건드린 버전은 -SkipMac 으로 먼저 확인 권장.
if ($SkipMac) { Write-Host "mac NAS 배포 생략(-SkipMac). 나중에 release.ps1 을 다시 실행하면 이어서 한다."; exit 0 }

$macZipName = "Mcapture-$ver-mac.zip"
Write-Host "태그 v$ver 의 CI 실행을 찾는 중..."
$runId = $null
for ($i = 0; $i -lt 12 -and -not $runId; $i++) {
    $runId = gh run list --workflow build --branch "v$ver" --event push --limit 1 --json databaseId --jq '.[0].databaseId'
    if (-not $runId) { Start-Sleep 10 }
}
if (-not $runId) { throw "태그 v$ver 의 CI 실행이 보이지 않습니다 — GitHub Actions 확인 후 재실행" }
Write-Host "CI run $runId 완료 대기 (Windows·macOS 빌드 + 릴리스 첨부, 보통 5~10분)..."
gh run watch $runId --exit-status --interval 20 | Out-Null
if ($LASTEXITCODE -ne 0) { throw "CI run $runId 실패 — 로그 확인: gh run view $runId --log-failed" }

$tmp = Join-Path $root "dist\ci-mac"
Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $tmp | Out-Null
gh release download "v$ver" --pattern $macZipName --dir $tmp
$macZip = Join-Path $tmp $macZipName
if (-not (Test-Path $macZip)) { throw "릴리스에서 $macZipName 을 받지 못했습니다" }

Copy-Item $macZip "$nas\$macZipName" -Force
# version-mac.txt: release-mac.sh 와 같은 형식 (버전 / zip 파일명, CRLF, UTF-8 BOM 없이)
[IO.File]::WriteAllText("$nas\version-mac.txt", "$ver`r`n$macZipName`r`n", (New-Object System.Text.UTF8Encoding $false))
Write-Host "mac 릴리즈 완료: v$ver → $nas\$macZipName + version-mac.txt (CI 빌드, run $runId)"
