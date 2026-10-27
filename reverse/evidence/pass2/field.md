# 第二遍：Field（地图格子与物件图）

## 0x0040B060 Field::LoadObjectPicture — exact

汇编：picture.data=0；按 0x004F25BC[obj]（压缩长度）分配，0x004F19BC[obj] 写入 picture+0x10 并分配；HUGE.lmf 定位 0x004F0DBC[obj]+0x004F01B8，读压缩数据；目标清零；0x00429C00 解压；释放压缩缓冲；首字 0xFFFF 时宽高格式取 +2／+4／+6，否则取 +0x202／+0x204／+0x206；格式 &0x3FFF==0x10 → 0x00401680（直接写 picture 宽高），否则 0x00401950；恒返回 1。
对照：MapObject 各列地址（offset 0x004F0DBC、packed 0x004F25BC、unpacked 0x004F19BC）、unitBase 0x004F01B8、调色板 0x200、Magic、CountMask 0x3FFF、TrueColor 0x10 一致；物件号 ≥0x300 的保护已登记（deviations「0x0040B060 越界」）。

## 0x0040B180 Field::ClearRoleCells、0x0040B270 Field::MarkRoleCells — diff → exact

汇编：行 = role.y 无符号右移 4，宽高 = role+0x6C／+0x70（act 'HW'）。清除掩码：id==0x004D1538 时 0xCF7FFEFF，否则 0xCF7FFFFF；标记位：0x10000000，flags&0x4000 再加 0x20000000，flags&0x10 加 0x800000，领队加 0x100。先改 cell[0]；高 1：宽 2 改 cell[1] 返回，宽 1 返回；高 2 且宽 1：改 cell[地图宽] 返回；否则高 ≤0 返回，逐行：行在 [0, 0x004D129C) 时列 = role.x 无符号右移 4，逐列在 [0, 0x004D1298) 时改；行指针每行加地图宽。
对照：ChangeRoleCells 的分支、捷径、有符号范围判断与位值一致（~0x30800100 = 0xCF7FFEFF）。
差异：格指针为空与越出格子数组的保护未登记。修正：补登 deviations「0x0040B180、0x0040B270 空指针、越界」。复核通过。

## 0x0040BF50 Field::ProbeCells — exact

汇编：32 项栈数组清零；上边基址 = cell−(地图宽+1)，下边 = cell+高×地图宽−1，左边 = 上边基址，右边 = cell+宽−地图宽；所有读值先 &mask。
- 宽 >2：i=0..宽+1，v[i] |= 上[i]、v[8+i] |= 下[i]；i=0..宽−1：上左 |= v[i]，上 |= v[i+1]，上右 |= v[i+2]，下左 |= v[8+i]，下 |= v[9+i]，下右 |= v[10+i]。宽 ≤2：读上下各 0..2，宽==2 再读 3；上左=v0、上=v1、上右=v2、下左=v8、下=v9、下右=v10；宽==2：上左=v0|v1，上=v1|v2，上右=v2|v3，下左=v8|v9，下=v9|v10，下右=v10|v11。
- 高 >2：i=0..高+1，v[16+i] |= 左[i×地图宽]，v[24+i] |= 右[i×地图宽]；i=0..高−1：上左 |= v[16+i]，左 |= v[17+i]，下左 |= v[18+i]，上右 |= v[24+i]，右 |= v[25+i]，下右 |= v[26+i]。高 ≤2：读左右各 0..2，高==2 再读 3；上左 |= v16、左=v17、下左 |= v18、上右 |= v24、右=v25、下右 |= v26；高==2：上左 |= v17，左=v17|v18，下左 |= v19，上右 |= v25，右=v25|v26，下右 |= v27。
- 结果位：上左 1、上 2、上右 4、右 8、下右 0x10、下 0x20、下左 0x40、左 0x80。
对照：源码在四条边不重叠的尺寸内与上述逐项一致，方向位与 BlockingMap 常量一致；四条边分开存放、格网外视为 0xFFFFFFFF 为已登记偏差（deviations 116、111），空格指针见 deviations「0x00403DC0… 空指针」。

## 0x0040D410 Field::CenterCamera — exact

汇编：left = roles[0x004C4B00].x − 0x130，<0 → 0；top = y − 0xF0，<0 → 0；right = left+0x280，≥ 宽<<4 → right = 宽<<4、left = (宽−40)<<4；bottom = top+0x1E0，≥ 高<<4 → bottom = 高<<4、top = (高−30)<<4（写 0x004C5118／511C／5120／5124）。
对照：CameraCentredOn 在 640×480、scale 1 时为 x−(320−16)、y−240，HoldCamera 先取 max(·,0)，再在 place+view ≥ extent 时取 extent−view，与上式相同；宽于 640×480 的视野为 deviations 93。

## 0x0040D4C0 Field::CameraAround — exact

汇编：rect.left = x−0x140（<0 → 0）、top = y−0xF0（<0 → 0）、right = left+0x280、bottom = top+0x1E0，越出地图时同 CenterCamera 夹紧。
对照：HoldCamera(x−viewWidth/2, y−viewHeight/2) 在 640×480 时一致；CameraRect 字段顺序 left/top/right/bottom 与 +0／+4／+8／+0xC 一致。

## 0x0040DFA0 Field::FindRegion — exact

汇编：从 0x004C6258 沿 +0 找 +4 == id 的节点，找不到返回 0。对照：一致。
