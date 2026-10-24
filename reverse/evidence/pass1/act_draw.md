# 第一遍汇编验证：ui/act_draw

## 0x0040EF60 ActDrawer::Draw、0x0040EFF0 DrawPicture、0x0040F030 DrawWithMode — exact（偏差补登、常量命名）

- Draw/DrawWithMode：Run 失败报 "Act Err(Misc::DrawActData)"/"設定錯誤 \nact[%d] qq[%d] dir[%d]"（0x004B1108、0x004B0D58，参数 +0、+8、+0x34）返回；GetPicture(+0x4A, +0x4C)，像素指针设为来源，透明度取字节 +0x92，DrawPicture(x−(+0x10), y−(+0x14), 宽 +0xC, 高 +0xE, 模式, 0)；Draw 的模式为 +0x18，DrawWithMode 为 (+0x18 & 0x80000003) | mode。DrawPicture：GetPicture(file, picture)，模式 0、参数 0 绘制于 (x, y)。
- 图片取不到时原程序照样经空指针读取：补登 deviations.md（实现不画）。
- 0x80000003 改为具名常量 kKeptModeBits（RleSource|Mirror|Flip），两个报错框注明字符串地址。
