# Stage 6 Reports

状态：current

更新日期：2026-09-23

本目录保存 Stage 6 各子批次的带日期实施和验证证据。当前阶段仍为 active；报告只记录实际
执行结果，不把 ADR 冻结、历史 hosted 结果或后续计划当作本批次实现证据。

## 报告

- [2026-09-20 S6-A1 基线、入口和证据矩阵](2026-09-20-s6-a1-baseline.md)
  （Debug/Release 基线、candidate 注册边界、代码入口盘点和缺口台账）
- [2026-09-21 S6-A2 冻结合同落盘与表征](2026-09-21-s6-a2-contracts-and-characterization.md)
  （Spec/Schema、API/安装草案、依赖边界、identity/media/config golden 和 focused checker）
- [2026-09-20 S6-B1 版本比较与发行门禁](2026-09-20-s6-b1-version-gate.md)
  （trusted baseline checker、UTC 递增规则、bootstrap 例外、workflow 合同和本地/hosted 证据边界）
- [2026-09-22 S6-C1 candidate source、CXC 与 typed lowering](2026-09-22-s6-c1-candidate-source.md)
  （实现当时的本地 MSVC 快照；该页不改写成退出证据）
- [2026-09-22 S6-C1 退出](2026-09-22-s6-c1-exit.md)
  （同 SHA hosted 默认 OFF 矩阵与批次退出边界；不是 Stage 6 关闭或 owner acceptance）
- [2026-09-22 S6-D1 渲染合同与测试 renderer](2026-09-22-s6-d1-renderer-contract.md)
  （实现当时的本地 MSVC 快照；该页不改写成退出证据）
- [2026-09-22 S6-D1 退出](2026-09-22-s6-d1-exit.md)
  （同 SHA hosted 默认矩阵与批次退出边界；不是 D2 或 Stage 6 关闭）
- [2026-09-22 S6-D2 OpenGL 迁移](2026-09-22-s6-d2-opengl-migration.md)
  （实现当时的本地 GPU smoke；该页不改写成退出证据）
- [2026-09-23 S6-D2 退出](2026-09-23-s6-d2-exit.md)
  （OpenGL 接线与 Player 正式帧的本地退出；该页记录退出当时的证据，不含之后的 MinGW 复验）
- [2026-09-23 S6-D2 最小化与恢复](2026-09-23-s6-d2-minimize-restore.md)
  （本机 smoke 自动最小化再恢复；drawable 保持 1280x720）
- [2026-09-23 S6-C2 Player support](2026-09-23-s6-c2-player-support.md)
  （实现当时的配置发布和默认路由 smoke；该页不改写成退出证据）
- [2026-09-23 S6-C2 退出](2026-09-23-s6-c2-exit.md)
  （进程锁、只读偏好、热拔插观察和命名设备打开的本地退出；`e01a4b1` hosted 已通过）
- [2026-09-24 S6-C3 退出](2026-09-24-s6-c3-exit.md)
  （命令表、应用状态机、固定事务顺序与最小键盘绑定的本地退出；真实窗口按键验证发现并修复了
  输入路径替换 bundle 时的 discontinuity delta 缺陷）

权威范围见 [Stage 6 计划](../../../stage_plans/active/stage-06/plan.md)、
[ADR 0042](../../../adr/0042-stage-6-productization-boundaries.md) 和
[CURRENT_STATUS.md](../../../CURRENT_STATUS.md)。
