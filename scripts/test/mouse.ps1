# 마우스 드래그/클릭 시뮬레이션 (화면 절대 좌표). 사용: mouse.ps1 drag x1 y1 x2 y2 | click x y | dblclick x y | rclick x y
param([string]$Op, [int]$X1, [int]$Y1, [int]$X2, [int]$Y2)
Add-Type @"
using System; using System.Runtime.InteropServices;
public class M {
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, IntPtr e);
  public const uint LD = 0x02, LU = 0x04, RD = 0x08, RU = 0x10;
}
"@
function Down { [M]::mouse_event([M]::LD, 0, 0, 0, [IntPtr]::Zero) }
function Up { [M]::mouse_event([M]::LU, 0, 0, 0, [IntPtr]::Zero) }
switch ($Op) {
  'drag' {
    [M]::SetCursorPos($X1, $Y1) | Out-Null; Start-Sleep -Milliseconds 120; Down; Start-Sleep -Milliseconds 80
    for ($i = 1; $i -le 12; $i++) { $x = $X1 + ($X2 - $X1) * $i / 12; $y = $Y1 + ($Y2 - $Y1) * $i / 12; [M]::SetCursorPos([int]$x, [int]$y) | Out-Null; Start-Sleep -Milliseconds 25 }
    Start-Sleep -Milliseconds 80; Up
  }
  'click' { [M]::SetCursorPos($X1, $Y1) | Out-Null; Start-Sleep -Milliseconds 120; Down; Start-Sleep -Milliseconds 60; Up }
  'dblclick' { [M]::SetCursorPos($X1, $Y1) | Out-Null; Start-Sleep -Milliseconds 120; Down; Up; Start-Sleep -Milliseconds 80; Down; Up }
  'rclick' { [M]::SetCursorPos($X1, $Y1) | Out-Null; Start-Sleep -Milliseconds 120; [M]::mouse_event([M]::RD, 0, 0, 0, [IntPtr]::Zero); Start-Sleep -Milliseconds 60; [M]::mouse_event([M]::RU, 0, 0, 0, [IntPtr]::Zero) }
  'move' { [M]::SetCursorPos($X1, $Y1) | Out-Null }
}
Write-Output "done $Op"
