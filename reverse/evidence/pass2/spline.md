# 第二遍：样条曲线

x87 精度为 53 位（MSVC 运行库默认），源码以 double 计算、按原次序累加并在原位置截为 float。

## 0x0043C990 WeighColumns — exact

汇编：对 4 列各求 m[c]·v0 + m[c+4]·v1，再加 m[c+12]·v3，再加 m[c+8]·v2，存为 float。对照：一致。

## 0x0043C9D0 Weighted — exact

汇编：x = p2.x·w2 + p1.x·w1 + p3.x·w3 + w0·p0.x；y = p0.y·w0 + p2.y·w2 + p1.y·w1 + p3.y·w3，各存为 float。对照：一致。

## 0x0043CA20 BasisWeights — exact

汇编：t² = t·t，t³ = t²·t，向量 (float t³, float t², t, 1.0)，0x0043C990(输出, 向量, 矩阵)。对照：一致。

## 0x0043CA70 Evaluate — exact

汇编：段 = ftol(floor(t) + 2.0)（0x004AA698 = 2.0），余 = float(t − floor(t))；0x0043CA20(余, 基矩阵 0x004BE010)；0x0043C9D0(权重, 点 + (段−2), 输出)。
对照：一致；基矩阵由 tests/spline_test.cpp 与 exe 字节比对。
