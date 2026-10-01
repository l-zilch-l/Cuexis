# ADR 0001：项目命名为 Cuexis

日期：2026-07-17

状态：已接受

## 背景

项目需要统一总名、运行程序、编辑器和 C++ 命名空间。

## 决策

项目总名为 Cuexis，共享核心称 Cuexis Engine，播放器称 Cuexis Player，编辑器称 Cuexis Studio，C++ 根命名空间为 `cuexis`。

## 备选方案

继续使用临时代号会导致 target、资产格式和发布名称反复迁移，因此不采用。

## 影响

构建产物、窗口标题、日志、命名空间和文档统一使用 Cuexis。

## 后续风险

若未来发生商标冲突，需要单独 ADR 和兼容迁移计划。

## SDK 转型补充（2026-07-20）

ADR 0027 将对外共享核心的正式产品名调整为 **Cuexis Playback SDK**。`Cuexis Engine` 只可作为阶段 0-1B 历史描述或内部模块总称，不再表示要独立完成通用游戏的产品。Cuexis、Cuexis Player、Cuexis Studio 和 `cuexis::` 命名保持不变。
