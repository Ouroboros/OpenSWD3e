# OpenSWD3e

《軒轅劍參外傳 天之痕》的重制执行程序。

它取代原版的 `swd3e.exe`，读取你已安装的原版游戏资料，在现在的 Windows 上直接运行。剧情、战斗、数值与各种规则都和原版一样，连原版的小 BUG 也保留；会让游戏崩溃的问题则已修正。

本项目不含任何游戏资料，需要你已有原版游戏。

主页：<https://ouroboros.github.io/OpenSWD3e/>

怪物资料见 [神魔異事錄](https://ouroboros.github.io/OpenSWD3e/bestiary.html)：仿游戏图鉴的网页，列出全部怪物的数值、五行承受伤害、法术、可偷的物品、掉落物品、狂暴条件和说明，也收录了游戏图鉴里没有的剧情对手，可以搜索。

## 安装与运行

1. 准备好原版游戏目录，例如 `E:\Game\swd3e`。
2. 解压后得到 `OpenSWD3e` 文件夹，里面有 `swd3er.exe`、`asset` 文件夹和设置文件 `swd3er.toml`。可以整个放在任意位置，也可以把里面的东西放进游戏目录。
3. 如果没有放进游戏目录，用记事本打开 `swd3er.toml`，在 `[paths]` 里写上游戏目录：

   ```toml
   [paths]
   data_dir = 'E:\Game\swd3e'
   ```

4. 运行 `swd3er.exe`。

和原版相比的改动、重制版新增的设置与原版的隐藏功能见[主页](https://ouroboros.github.io/OpenSWD3e/)。每一处和原版不同的行为都记录在 [deviations.md](reverse/analysis/deviations.md)。

## 从源码编译

见 [docs/building.md](docs/building.md)。

## 网页的生成

- `docs/index.html`：手写的主页。
- `docs/bestiary.html`：由 `scripts/bestiary/` 生成。`dump.cpp` 链接 `swd3e_core`，从游戏目录读出 mon.dat 的怪物资料与图鉴立绘（动作 0x3B 的第一帧）；`build.py`（需要 Pillow）把它们做成单个 HTML 文件，图片内嵌。用法见两个文件开头的说明。
