# 第一遍汇编验证：PathNodePool 与 PathFinder（0x00401B30–0x00402A90）

结论均为 exact。每条记录从汇编推导的要点。

## 节点池

- 0x00401B30 Init：Allocate(0x58) 作池头（清零后 +0x28 置 0），free(0x004C15C0)=0，do-while 追加 8 个清零节点到头部链表，最后 free=8、peak(0x004C15C4)=8、unused(0x004C15BC)=0。
- 0x00401BC0 Release：逐个摘下池头链表的节点释放并 free--，再释放池头、池头指针清零，返回 free==0。实现以 storage 统一持有内存，未归还节点在原程序泄漏（无可见行为）。
- 0x00401C20 Alloc：空池时新建清零节点放入链表，free++，peak<free 时 peak=free；然后摘下首节点、free--、整节点 0x58 字节清零后返回。
- 0x00401CA0 Free：空指针返回 0；否则节点放回链表头，返回 ++free。

## 寻路器

- 0x00401CD0 构造：+0x10 栈表头（8 字节清零块）、+0x14 found、+0、+4、+8 清零，掩码 +0x28 = 0x40800000（Wall|Unk800000）。+0x0C、+0x20 起的字段不初始化，均在读取前写入（Search 写 closest，FindPath 写宽高，UpdateBlocked 写阻挡位）。
- 0x00401D00 析构：Reset 后释放栈表头。
- 0x00401D30 FindPath：缓冲首字节写 0xFF，宽高写入 +0x20/+0x24；起终点相同返回 1（不动 found）；终点不可走或起终点同格：found=0、Reset、返回 0；否则 Reset、Search，IsUsable 为 0 返回 0（不 Reset）；沿 parent 链逐步写方向（首步覆盖开头的 0xFF），步数超过 0x1FE 时每步弹一次提示（"!!! 危險 步數超過510 !!!"/"路徑計算"，0x004AF0F8/0x004AF114，已比对字节），末尾写 0xFF、found=1、Reset、返回 1。
- 方向表：0x004AA320 DirectionByDelta {0,1,2,7,0,3,6,5,4}、0x004AA32C DeltaX {-1,0,1}、0x004AA338 DeltaY {-3,0,3}、0x004AA344 OppositeDirection {4,5,6,7,0,1,2,3}；新增单元测试“path finder: the direction tables match swd3e.exe”比对 exe 字节，头文件注释由 swd32 地址改为 swd3e 地址。
- 0x00401E80 IsUsable、0x00401EB0 TileIndex（0x004D1298 地图宽）、0x00401ED0 IsWalkable（0x004D12B0 格子 & 掩码为 0）。
- 0x00401F00 Reset：开表哨兵为空即返回；否则逐个归还哨兵 poolNext 链上的节点，再归还哨兵，开表指针清零。
- 0x00401F40 Search：取起点格；分配开表、闭表哨兵与目标节点并串成 poolNext 链；目标 g=0、h=f=sqrt(dx²+dy²)（整数平方和 fild/fsqrt，53 位精度与 double 一致）；开表首项为目标、closest=目标；循环 PopBest，到起点格时 result=该节点返回，否则 Expand；开表空时 result=closest。
- 0x00402020 PopBest：开表空时写错误日志 "aStar ,Path find fail"（0x004AF120，经 0x00411F10）返回 0；否则摘下首项放到闭表头。
- 0x00402060 Expand：先 UpdateBlocked，再按 NW(+0x48)、N(+0x2C)、NE、E、SE、S、SW、W(+0x44) 顺序对未阻挡的方向调用 TryNeighbour，方向编号 0–7。
- 0x00402180 TryNeighbour：g = sqrt(dy²+dx²)+parent.g；开表中找到：挂为子节点、更新 closest（closest.h > node.h 时）、g 更小时改 g、parent、f=g+h；闭表中找到：同上并在 g 更小时 Propagate，之后无论如何把 OppositeDirection[方向] 写入 parent 第一个子节点的 direction；都没有：新节点挂入开表哨兵 poolNext 链，填 g、parent、x、y、tile、direction、h=sqrt((x-tx)²+(y-ty)²)、f=h+g，先 InsertOpen 再挂子节点、更新 closest。子节点槽 8 个，满则不挂。
- 0x00402460 FindOpen、0x00402480 FindClosed：沿 listNext 找 tile 相同的节点。
- 0x004024A0 InsertOpen：开表空时直接挂上；否则插在第一个 f ≥ node.f 的节点之前（C0|C3 判定）。
- 0x004024E0 Propagate：以入口时 node.g 的副本对各子槽加 0x004AF0B0 的步长（偶数槽 1.414214、奇数槽 1.0，已比对字节），g 更小的子节点更新 g、f、parent 并入栈；随后反复出栈，对出栈节点（读当前 g）做同样处理直到栈空。
- 0x004025B0 Push、0x004025E0 Pop：+0x10 表头上的单链 LIFO。
- 0x00402600 UpdateBlocked：八个阻挡位清零；上下行起点 (y-16)>>4 与 (y>>4)+高，左列 (x-16)>>4、右列 x>>4+宽；宽或高 ≤2 时展开读取 3 格（=2 时 4 格）并按 1 格／2 格规则合并，>2 时读取 宽+2 格并对每个脚印格合并三个方向。各分支的数组下标与阻挡字段逐一对照一致。四个 8 项栈数组超过 6 格时重叠写出、地图边缘读出数组之外，见 deviations.md。
- 0x00402A90 SetMask：写 +0x28，返回参数。
