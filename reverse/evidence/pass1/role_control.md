# 第一遍汇编验证：RoleControl 其余函数（0x00403DC0–0x00404DD0）

结论均为 exact。

## 0x00403DC0 Slide

- ProbeCells(role.cell, 宽, 高, 0x60800000) 得探测位（UpLeft=1、Up=2、UpRight=4、Right=8、DownRight=0x10、Down=0x20、DownLeft=0x40、Left=0x80）。
- 栈上 8 张规则表（0x00403DC9 起初始化）：左 {-1,2,1,0x80,0x40,0x20}、右 {宽,2,4,8,0x10,0x20}、上 {-1,0x80,1,2,4,8}、下 {高,0x80,0x40,0x20,0x10,8}、左上 {-1,1,0x80,2}、右下 {1,0x10,8,0x20}、左下 {-1,0x40,0x80,0x20}、右上 {1,4,8,2}，与 kSlide* 逐项一致。
- 横向直步：FreeLanes(表[0]+x>>4, y>>4, 高, 地图宽, 地图高, 1)；为 0 则 stepX=0；之后依次判 0x80/表[3]（直行）、0x40/表[2]（stepY=-1）、0x100/表[4]（stepY=1）、0x3F/表[1]（stepX=0,stepY=-1）、0xFE00/表[5]（stepX=0,stepY=1），都不行则两者清零。纵向同理（FreeLanes(x>>4, 表[0]+y>>4, 宽, 1, 地图宽, 0)，改 stepX）。
- 斜步：stepX+stepY≠0 用左上/右下表，否则左下/右上表；对角不挡则不变，横向不挡则 stepY=0、stepX=表[0]，纵向不挡则 stepX=0、stepY=±表[0]（和为 0 时取负），否则清零。两步皆 0 的分支只有调用者保证不进入。

## 0x00404220 FreeLanes

- 起点 = 格子 (y·地图宽+x)，沿轴坐标取 axis==1 ? y : x。shift 0..2：坐标−shift ≥0 时扫描后方车道（length 格、间隔 stride），格子 byte3&0x60 有则不通，置 0x80>>shift；坐标+shift+length < limit 时扫描前方车道，置 0x80<<shift。
- 格网之外按阻挡处理（deviations.md 0x00404220）。

## 0x00404320 StepEvent

- found 先置 -1，返回值初值 -1（无步进或尺寸为 0 未读任何格子时返回 -1）。分支号 = stepX+3·stepY+4。
- 逐格读区域号（格子 &0xFF），非 0 即返回；否则查 FindTouchRoleAt，找到则写 found 并返回 0；扫描完返回最后读到的区域号（0）。
- 八个方向的起点与触碰坐标逐一对照：左上 行(cell−地图宽−1; tx−1+i, ty−1) 再列(同起点; tx−1, ty−1+i)；上 行(cell−地图宽; tx+i, ty−1)；右上 行(cell−地图宽+1; tx+1+i, ty−1) 再列(cell+宽−地图宽; tx+宽+1, ty−1+i)；左 列(cell−1; tx−1, ty+i)；右 列(cell+宽; tx+宽, ty+i)；左下 行(cell+地图宽·高−1; tx−1+i, ty+高) 再列(cell+地图宽−1; tx−1, ty+1+i)；下 行(cell+地图宽·高; tx+i, ty+高)；右下 行(cell+地图宽·高+1; tx+1+i, ty+高) 再列(cell+宽; tx+宽, ty+1+i)。
- 原有 BUG 保留：列扫描每步格子指针加角色宽度（`lea ebp,[ebp+esi*4]`，esi 为宽）而非地图宽度；右上列的触碰 x 为 tx+宽+1 而格子列为 tx+宽。
- 格网之外的区域号视为 0（deviations.md）。

## 0x00404920 FindRoleAhead

- 可达距离：横向 5，x≤5 时取 x，地图宽−x≤5 时取后者（覆盖前者）；纵向同理（无符号比较）。
- 分支（stepX+3·stepY+4）：左上 far 1..ny、side 1..nx（x−side, y−far）；上 far 1..ny、side 0..宽+1（x−1+side, y−far）；右上 far 1..ny、side 1..nx−1（x+side, y−far）；左 far 1..nx、side 0..高+1（x−far, y−1+side）；右 far 0..nx−1、列 x+宽+far；左下 far 1..ny−1、side 1..nx（x−side, y+far）；下 far 0..ny−1、行 y+高+far；右下 far 1..ny−1、side 1..nx−1（x+side, y+far）。范围的不对称保留。
- 所有分支（含无步进）之后扫描角色自身脚印（x+列, y+行），都没有返回 -1。

## 0x00404CF0 FindRoleAt、0x00404DD0 FindTouchRoleAt

- 角色 1..roleCount−1（无符号），event（+0x1E）非 0 且旗标 0x8000，脚印（x>>4、y>>4、宽 +0x6C、高 +0x70，无符号）覆盖 (x,y) 时返回序号；否则 -1。
- FindTouchRoleAt：结果非 -1 且旗标无 0x2000 时返回 -1。
