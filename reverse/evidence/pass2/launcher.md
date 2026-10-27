# 第二遍：启动器

## 0x00428210 Launcher::OnClose（对话框过程 0x6B）— diff → exact

汇编：WM_INITDIALOG：对话框按屏幕居中（640×480），背景控件 0x405 置 (0,0,640,480)；五个高亮控件 0x40E–0x412 以 SetWindowPos(…, 0x70×0x28, 旗标 0x84 = SWP_HIDEWINDOW|SWP_NOZORDER) 放到 (0xB8,0x140)、(0xB8,0x168)、(0x160,0x140)、(0x160,0x168)、(0x110,0x1A0) 并隐藏；两组高亮字节清 0；SetFocus；返回 1。WM_CLOSE：EndDialog(6)。WM_COMMAND（低字）：0x3FE → 对话框 0x70 后 EndDialog(2)；0x3FF → 0x00428750("www.softstar.com.tw")；0x400 → 0x00428880(目录+"Readme.txt")；0x401 → EndDialog(6)；0x402 → EndDialog(3)。WM_MOUSEMOVE/WM_LBUTTONDOWN：上次高亮 ← 本次，本次清 0；坐标取 lParam 低/高字（无符号）；依次判断五个矩形（宽 0x70、高 0x28；第一个还要旗标 0x12），命中则高亮 = 5、上次为 0 时记“新亮”，wParam == 1 时分别 EndDialog(1)/(2)/打开网址/打开说明/EndDialog(6)（不中断后续判断）；五个控件按高亮字节 ShowWindow；有新亮时 PlaySoundA(资源 0x77, 0x42007)。
对照：命中矩形、旗标 0x12、选择结果、命令、新亮音效一致（Layout、ItemWidth、ItemHeight、SavedGameFlag、HoverSound）；对话框、ShowWindow 与 PlaySoundA 为已登记的平台替换（deviations 71、73）。
差异/修正：源码 `shown` 初值为全部可见（沿用正传），外传 WM_INITDIALOG 以 SWP_HIDEWINDOW 隐藏五个高亮；Init 改为清空 shown，初值改为全部隐藏。复核：完整函数重新对照一致。

## 0x00428750 Launcher::RunBrowser — exact

汇编：从 HKEY_CLASSES_ROOT\http 与 http\shell\open\command 读命令行，截去最后一个空格后的部分接上网址，WinExec。
对照：平台替换（deviations 72），交给系统默认处理程序。

## 0x00428880 Launcher::RunTextViewer — exact

汇编："notepad.exe " + 路径，WinExec(…, 9)。对照：平台替换（deviations 72）。
