# Demo：20 秒旋转逼近示例（demo20s）

状态：active（运行示例）

更新日期：2026-09-01

## 说明

一个可由 `cuexis_player --project` 加载并在屏幕上播放的运行示例 Source Project：

- 项目目录：`assets/projects/demo20s/`
- 打包产物：`assets/projects/demo20s.cxc`
- 内容：600×600 图片贴在四边形上，20 秒内绕竖直轴（Y）旋转 7200° 并向 z=5 的摄像机逼近。

## 动作细节

- **旋转**：`behavior.event` 的 `transform.rotation`，用 40 段 × 180° 实现 7200°（四元数
  shortest-path slerp 单段上限 180°，故拆分；相邻段点积为 0，无符号翻转歧义）。
- **移动**：`transform.position.z` 从 0.0 逼近到 4.0（摄像机在 z=5，物体向 +Z 靠近）。
- **时长**：`defaultBpm=120`，40 拍 = 20 秒；结束后旋转停止、物体停在 z=4。

## 运行（需 GPU / 窗口）

```powershell
.\out\build\debug\bin\cuexis_player.exe --project .\assets\projects\demo20s
```

## 已知边界

- Player 直接加载的是 Source Project（`--project`）或 Chart JSON（`--chart`）；`.cxc` 是
  `cuexis_cxc_pack` 产出的交换包，Player CLI 不直接消费 `.cxc`。
- 纹理必须是 `CXPRES01` `.texture.bin`；当前引擎无 jpg/png 解码，源 `test.jpg` 需先转成
  `.texture.bin`（见 [核验记录问题 3](../stage_reports/reviews/stage-verification-2026-09/2026-09-01-findings.md)）。
