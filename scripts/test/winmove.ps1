# 창을 지정 위치로 옮기고 앞으로 가져온다. 사용: winmove.ps1 <제목일부> <x> <y> [close]
param([string]$Title, [int]$X, [int]$Y, [string]$Op)
Add-Type @"
using System; using System.Runtime.InteropServices; using System.Text;
public class WM {
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc cb, IntPtr lp);
  [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  public delegate bool EnumWindowsProc(IntPtr h, IntPtr lp);
  public static IntPtr Find(string part) { IntPtr f = IntPtr.Zero;
    EnumWindows((h, lp) => { if (!IsWindowVisible(h)) return true; var sb = new StringBuilder(256); GetWindowText(h, sb, 256);
      if (sb.ToString().Contains(part)) { f = h; return false; } return true; }, IntPtr.Zero); return f; }
}
"@
$h = [WM]::Find($Title)
if ($h -eq [IntPtr]::Zero) { Write-Output "NOTFOUND"; exit 1 }
if ($Op -eq 'close') { [WM]::PostMessage($h, 0x10, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null; Write-Output "closed"; exit 0 }
[WM]::SetWindowPos($h, [IntPtr]::Zero, $X, $Y, 0, 0, 0x0001 -bor 0x0004 -bor 0x0040) | Out-Null   # NOSIZE|NOZORDER|SHOWWINDOW
[WM]::SetForegroundWindow($h) | Out-Null
Write-Output "moved"
