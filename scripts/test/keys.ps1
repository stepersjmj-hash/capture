# 키 입력 전송 (SendKeys 표기). 사용: keys.ps1 "^s"  — 별도 프로세스라 포그라운드가 안 바뀐다
param([string]$Keys)
$ws = New-Object -ComObject WScript.Shell
$ws.SendKeys($Keys)
Write-Output "sent"
