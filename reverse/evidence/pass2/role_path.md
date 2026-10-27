# 第二遍：role_path（路径搜索与节点池）

方法见 reverse/analysis/pass2-method.md。

## 0x00401B30 PathNodePool::Init — exact

汇编：分配并清零 0x58 字节的池头（0x004C15C8），头的 +0x28 置 0；空闲数 0x004C15C0 置 0 后 do-while 新建 8 个清零节点，逐个插到头后、空闲数加一；结束后空闲数与峰值 0x004C15C4 置 8，0x004C15BC 置 0。
对照：一致（源码 do-while，storage 只在启动时调用一次，game.cpp:867）。

## 0x00401BC0 PathNodePool::Release — exact

汇编：逐个摘下头后的节点并释放、空闲数减一，释放头、头指针置 0，返回 (空闲数==0)。
对照：一致（源码摘链计数后统一释放；只在退出时调用，game_window.cpp:172）。

## 0x00401C20 PathNodePool::Alloc — exact

汇编：池空时新建清零节点插到头后、空闲数加一，峰值 < 空闲数时峰值=空闲数；摘下头后第一个节点，空闲数减一，整块 0x58 字节清零后返回。
对照：一致（PathNode{} 各字段为 0）。

## 0x00401CA0 PathNodePool::Free — exact

汇编：参数为空直接返回（eax=0）；否则插到头后，空闲数加一，eax 为新空闲数。
对照：一致（源码返回值与 eax 相同）。

## 0x00401CD0 PathFinder::PathFinder — exact

汇编：+0x10 分配 8 字节并清零（栈），+0x14、+0x00、+0x04、+0x08 置 0，+0x28 置 0x40800000（Wall|Unk800000 掩码），返回 this。+0x0C、+0x18..+0x24、+0x2C 起未写。
对照：一致。未写字段在源码中初值为 0；是否在写入前被读，在各读取函数的行核对。

## 0x00401D00 PathFinder::~PathFinder — exact

汇编：调用 0x00401F00（Reset），+0x10 非空时释放并置 0。
对照：一致（vector 自行释放）。

## 0x00401D30 PathFinder::FindPath — exact

汇编：dirs[0]=0xFF；+0x20/+0x24 写宽、高；起点==终点返回 1（不改 found、不 Reset）；终点不可走或两点同格：found=0、Reset、返回 0；否则 Reset、Search(from, to)；IsUsable 为 0 时直接返回 0（不改 found、不 Reset）；从 result 沿 parent 逐步写方向：DirectionByDelta[DeltaY[dy+1]+DeltaX[dx+1]+4]（dx、dy 为当前减父节点的格坐标，sar 4），计数 >0x1FE 时每步 MessageBoxA("!!! 危險 步數超過510 !!!", "路徑計算") 且继续写；写 0xFF，found=1，Reset，返回 1。
对照：一致。文字与 0x004AF0F8、0x004AF114 的 Big5 原文一致；写出 512 字节之外见 deviations.md 0x00401D30 行。

## 0x00401E80 IsUsable、0x00401EB0 TileIndex、0x00401ED0 IsWalkable、0x00401F00 Reset — exact

- IsUsable：result 为空返回 0；found==0 返回 1；否则 (result.parent==0)。
- TileIndex：(y sar 4)·地图宽(0x004D1298) + (x sar 4)，32 位。
- IsWalkable：格值(0x004D12B0 数组)&掩码(+0x28) 为 0 返回 1，否则 0（`neg/sbb/inc`）。地图外见 deviations.md 第 51 行。
- Reset：open 为空返回；逐个摘下 open 链（poolNext）还给池，再还 open，置空。
对照：均一致。

## 0x00401F40 Search — exact

汇编：起点格 = TileIndex(from)；分配 open、closed、目标三个节点并串成 poolNext 链；目标 g=0、poolNext=0，h=f=fsqrt(fild(dy·dy+dx·dx))（32 位整数平方和，按像素），tile=TileIndex(to)，x、y=to；open.listNext=目标，closest=目标；循环 PopBest：空则 result=closest 返回 0；tile==起点格则 result=该节点并返回它；否则 Expand(节点, from)。
对照：一致。

## 0x00402020 PopBest — exact

汇编：open 表空时 ErrorLog("aStar ,Path find fail\n")（0x004AF120，0x00411F10 为空函数）返回 0；否则摘下首个，插到 closed 表头，返回。
对照：差异——源码日志文字缺少原串末尾的 `\n`，已补上（AGENTS.md：日志控制字符照原样保留）。

## 0x00402060 Expand — exact

汇编：UpdateBlocked(node.x, node.y)；按 blocked[7]、[0]、[1]…[6]（+0x48、+0x2C…+0x44）为 0 依次 TryNeighbour，坐标偏移 (−16,−16)、(0,−16)、(16,−16)、(16,0)、(16,16)、(0,16)、(−16,16)、(−16,0)，方向号 0..7。
对照：一致。

## 0x00402180 TryNeighbour — exact

汇编：g = fsqrt(dy²+dx²)+parent.g（dx、dy 为邻格减父节点，像素）；tile；在 open 中：挂为父节点首个空子槽，closest.h > 节点.h（`test ah,41h`）时 closest=节点，g < 节点.g（`test ah,1`）时更新 g、parent、f=g+h，返回；在 closed 中：同样挂接、比较，更新时另调 Propagate，然后 parent.children[0].direction = 0x004AA344[dir]；都不在：新节点挂到 open 的 poolNext 链，g、parent、x、y、tile、direction，h=fsqrt(dx²+dy²)（邻格减 target），f=h+g，InsertOpen，挂接，比较 closest。
对照：一致（运算顺序、比较方向与源码相同）。

## 0x00402460 FindOpen、0x00402480 FindClosed — exact

汇编：沿 listNext 查 tile 相等的节点，无则 0。对照一致。

## 0x004024A0 InsertOpen — exact（比较方向改为与汇编相同）

汇编：open 表空时直接接上；否则从首个起，`fcom` 后 `test ah,41h` 为 0（节点.f 大于当前.f）时后移，小于、等于或无序时停下，插在其前。
对照：源码 `!(node.f <= current->f)` 在 NaN 时继续后移，与汇编在无序时停下不同；改为 `node.f > current->f`（f 不会为 NaN，行为不变，写法与汇编同向）。

## 0x004024E0 Propagate — exact

汇编：先用 node.g 的副本逐子槽（遇空槽即停）计算 g+0x004AF0B0[槽]，小于（或无序）子节点 g 时更新 g、f=g+h、parent 并压栈；然后栈非空时逐个弹出，用其 g 同样处理子节点。
对照：一致（node 不会是自己的子节点，副本与直接读等价）。

## 0x004025B0 Push、0x004025E0 Pop — exact

汇编：Push 分配 8 字节单元 {节点, 栈顶} 作为新栈顶；Pop 取栈顶单元，栈顶改为其 next，释放单元，返回节点。后进先出。对照一致（vector）。

## 0x00402600 UpdateBlocked — exact

汇编：blocked 八项清零；tx=x sar 4，above=(y−16) sar 4；上行从 above·W+tx−1、下行从 ((y sar 4)+高)·W+tx−1 开始。宽≤2（有符号）时：上下各取 3 格，宽==2 再取第 4 格；NW=上0、N=上1、NE=上2、SW=下0、S=下1、SE=下2，宽==2 时各为相邻两格之或。宽>2 时：上下各取宽+2 格，j=0..宽−1 逐项或入 NW←上j、N←上j+1、NE←上j+2，下行同理。左列从 above·W+((x−16) sar 4)、右列从 above·W+tx+宽，高≤2 时各取 3 格（高==2 取 4 格），NW|=左0、W|=左1、SW|=左2、NE|=右0、E|=右1、SE|=右2，高==2 再或入下一格；高>2 时各取高+2 格，逐项或入。
对照：一致（宽、高≤0 按 1 处理，右列按原宽）。8 项栈数组的重叠见 deviations.md 第 52 行；地图外见第 51 行。

## 0x00402A90 SetMask — exact

汇编：+0x28 = 参数，返回参数。对照一致。

## 构造函数未写字段

+0x0C closest 在 Search 中先写后读；+0x20/+0x24 在 FindPath 中先写，UpdateBlocked 才读；+0x2C 起的 blocked 由 UpdateBlocked 先清零；+0x18/+0x1C 在本组函数中不被读。
