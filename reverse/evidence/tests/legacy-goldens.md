# 正传遗留黄金结果测试的处理

swd3-rebuild 的黄金结果（tests/golden/*_golden.txt）由其遗留实现在 swd32 的游戏数据上记录。对 swd3e 而言，黄金结果只在“被测函数与 swd32 代码相同”时仍是有效依据；函数是否相同以 reverse/functions.tsv notes 中的配对方法（`method=hash` 为逐字节相同）为准。测试结果只辅助验证，不代替逐函数汇编对照。

## 保留

| 测试 | 数据 | 依据 |
| --- | --- | --- |
| text font: matches the golden results of the legacy renderer | 不读游戏数据 | src/font/ 29 个函数中 28 个 hash 配对；69 个情景全部一致 |
| projector: matches the golden results of the legacy projector | swd32 数据（`SWD3_LEGACY_DIR`，默认 /mnt/e/Game/swd3） | src/pic/projector* 13 个函数全部 hash 配对；769 个情景一致（非 MSVC rand 序列时跳过爆炸情景） |
| painter: matches the golden hashes of the legacy drawing kernel | swd32 数据 | 1300 个情景一致；排除 24 个：未缩放阴影在首个裁剪下依赖 DrawPicture（0x00417A60，外传改写）在两次绘制间保留的放置状态 |
| rle: matches the golden hashes of the legacy codec | swd32 数据 | 64 个情景一致；排除编码情景 19 个：RleCodec::Encode（0x00401000）外传改写（行旗标与填充段） |
| projector: the colour keys are compared unconverted / the first explosion frame counts the opaque pixels | swd3e 数据 | 不依赖黄金结果，在 swd3e 数据上通过 |

## 删除

| 测试 | 原因（在 swd32 数据上运行的差异数） |
| --- | --- |
| act: matches the golden results of the legacy archives and ActRun | act 档案与动作解释器按外传重写（hash 配对 3/18），111 个差异 |
| tsw: matches the golden results of the legacy archives | tsw 档案按外传重写（hash 配对 3/20），158 个差异 |
| path finder: matches the golden results of the legacy path finder | 寻路器按外传核对改写（hash 配对 1/23），247 个差异 |
| ui drawing: matches the golden results of the legacy boxes and frames | 133 个差异（框与边框改写，含外传图片编号） |
| battle hud: matches the golden results of the legacy helpers | HUD 按外传改写（hash 配对 12/39），全部图片情景不一致 |
| database: mon.dat objects and skills, level.dat entries match the legacy readers | 外传 mon.dat 记录格式不同，257 个差异 |

其余原列为待外传化的测试已改为对照 swd3e.exe 或 swd3e 数据（存档、maps.dat）通过，tests/pending_swd3e.txt 已清空。
