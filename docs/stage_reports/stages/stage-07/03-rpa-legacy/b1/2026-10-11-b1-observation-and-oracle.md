# B1.1/B1.2 观测身份与独立验证设施

状态：implementation evidence；B1.1/B1.2基础设施已实施，RPA产品协议未完成

证据日期：2026-10-11（Asia/Shanghai）。本记录只覆盖测试设施和其定向验证，不把
B3执行协议、B4宿主资源协议或B5实体设备验收记为完成。

## 范围

本批次在 `tests/rpa/` 新增隔离的 Catch2 target `cuexis_rpa_tests`，不向 Playback、
Judgement 或安装公共头加入尚未冻结的 RPA API。设施包含：

- `EventIdentity` / `TraceEvent`：保留 `sessionScope`、`runEpoch`、`clockSegment`、
  `deliverySequence`、阶段、事件种类，以及分开的 `commandOrdinal` 和 `decisionOrdinal`。
  测试身份不复用 canonical 输入或 I1/Fact 身份。
- `EvidenceManifest`：绑定源码 HEAD、dirty diff hash、语义 revision、配置、内容/硬件身份、
  缺样计数、观测事件数和观测开销；以稳定文本输出，便于后续证据归档。
- `FakeClock`：可控单调时间、publication sequence、segment、validity、error bound 和显式
  invalid/new-segment 转换。
- `ControlledDelivery`：捕获序号与交付序号分离，可按指定顺序交付并保留原 canonical ordinal。
- 独立 `Rational` / `roundHalfEven` / `mapTime`：checked 有理数、一次 offset 应用、负值与
  half-even 舍入；不调用生产 `advance` 或 RuntimeFrame double。
- `scanExecutionOracle`：独立扫描输入和观察序列，给出批关闭、执行顺序、Fact/Fold 结果；
  不读取生产 cursor、reducer 或执行函数。

## 实际验证

开工基线为 branch `stage-7`、HEAD `7ca1fc6cf2dd4c7072aab3ae260ee5931f5df2d3`、
SDK `0.7.5` / date build `26.10.10-1`；工作树既有未提交改动继续保留。新设施加入后使用
`mingw-headless-debug`（实际编译器为 `C:/msys64/ucrt64/bin/x86_64-w64-mingw32-g++.exe`）执行：

```text
cmake --preset mingw-headless-debug --fresh                 PASS
cmake --build --preset mingw-headless-debug --target cuexis_rpa_tests PASS
ctest --test-dir out/build/mingw-headless-debug -L rpa --output-on-failure PASS (6/6)
```

六项通过测试覆盖事件身份、manifest 缺样/开销字段、可控交付重排、fake clock 有效期与分段、
有理数时间映射/舍入，以及独立扫描 oracle 的闭批/执行/Fact/Fold 结果。

## 环境限制与未执行项

直接使用 `headless-debug` 曾因当前 PowerShell 选择 GCC 而 preset 固定 `x64-windows` 被 CMake
按仓库保护规则拒绝；随后使用匹配的 MinGW preset 完成了本批验证。未执行完整 Debug/Release
矩阵、Release基线、真实 clock drift、尾段/容量/性能、跨线程 owner、音频/GPU、实体设备或
hosted 验证；这些仍归 B1.3/B1.4、B2、B3、B4、B5 和 F1–F4。

本批次未提交、未推送、未创建 PR，也未修改既有未提交源码/测试改动。
