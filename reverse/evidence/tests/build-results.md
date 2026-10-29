# 构建与测试记录

构建结果、测试结果与逐函数汇编验证结论分开记录；测试结果只作辅助，不代替逐函数对照（证据见 reverse/evidence/pass1/）。

## Linux（x86_64 静态，WSL）

- 入口：`cmake --preset linux`、`cmake --build --preset linux`（构建目录 build/linux/，工具链 cmake/toolchain-linux-x64-clang.cmake）；单元测试 `ctest --preset linux` 或直接运行 `build/linux/swd3e_tests`。
- 结果（第二遍 1710／1710 完成时的工作树，2026-10-29）：构建成功；单元测试 172 项全部通过（`172 test cases, 0 failures, 0 pending swd3e`）。正传黄金结果的处理见 legacy-goldens.md。

## Windows（x64，WSL 中以 clang-cl／lld-link 交叉构建）

- 入口：`cmake --preset windows`、`cmake --build --preset windows`（构建目录 build/windows/，工具链 cmake/toolchain-windows-x64-clang-cl.cmake，MSVC 14.44 与 Windows SDK 10.0.26100 的头文件和库）。
- 产物：build/windows/swd3er.exe（游戏），build/windows/swd3e_tests.exe（单元测试），build/windows/swd3er.toml（开发用配置，不入库：data_dir = E:\Game\swd3e，save_dir = E:\Game\swd3e\save，日志 logs/swd3er-<日期_时间>-<PID>.log），build/windows/asset/。
- 结果（同上）：构建成功，0 个编译警告（`grep -c 'warning:' tmp/build_windows.log` 为 0）。
- Windows 单元测试与试玩由用户运行，结果回传后记入本节。

| 项目 | 状态 |
| --- | --- |
| Windows 单元测试（swd3e_tests.exe） | 待用户运行 |
| Windows 试玩（swd3er.exe） | 待用户运行 |
