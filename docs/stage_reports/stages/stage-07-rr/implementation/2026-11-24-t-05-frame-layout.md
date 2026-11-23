# T-05 RR-3/R5 像素几何与 GL 边界

状态：evidence

<!-- cuexis-meta
id: REPORT-stage-07-rr.t-05-frame-layout
status: evidence
authority: none
date: 2026-11-24
-->


任务：`task-18` / T-05（卡片：`docs/stage_plans/active/stage-07-rr/tasks/T-05.md`，标题「RR-3/R5 像素几何与 GL 边界」）
工作树：`E:\Cuexis-worktree`，基线 HEAD `8cfb302d`；本卡开始时工作树干净。
执行日期：2026-11-24。
本卡只做合同点名的几何、投影组合和借用 FBO 的 GL helper；不接 materializer、新 Reader、成品或 Player CLI。

工作树在并行波次期间出现了其它任务的未提交改动（例如 `docs/HANDOFF.md`、`engine/playback/src/playback_session.cpp`、`tests/playback/CMakeLists.txt` 中的 T-17 用例等）。本卡没有修改这些文件；构建和测试在当时的混合工作树上执行。下文的“成立”只针对本卡新增/修改的代码和本卡证据。

## 结论

| 项 | 结论 | 依据 |
| --- | --- | --- |
| R5-01/03/04/06/09 几何、整数比例、空值与选择 | 成立 | `rr_frame_layout_geometry`、`rr_frame_layout_validation` 通过；日志 `ctest-rr_frame_layout.log` |
| R5-10 Q/R/Pout、四旋转、像素映射与完整逆算 | 成立（纯值） | `rr_frame_layout_projection` 通过；608 条断言含投影与逆算 |
| R5-02 借用 FBO 的宿主 clear、内容绘制、区域外 sentinel、状态归还 | 成立 | `rr_frame_layout` 的真实 GL 3.3 上下文，像素与状态断言通过 |
| R5-11 附件校验、color0 路由、draw route 恢复、draw failure 恢复 | 成立（除下一行） | 合法 color0+depth、color1-only、缺 depth、texture、depth-stencil、多样本、尺寸不符均取证 |
| R5-11 “不完整 FBO 报 `framebuffer_incomplete`” | 无法核实 | 本机驱动对“draw/read buffer 指向缺失附件”仍报 `GL_FRAMEBUFFER_COMPLETE`（0x8CD5）；在既有附件校验之后无法构造该分支 |
| debug 构建、CTest 注册、静态默认与 headless 纯值测试 | 成立 | 精确注册 `rr_frame_layout`，前缀另列 3 个纯值用例；4/4 与 3/3 通过 |
| 默认 shared 与 candidate-shared 逐符号导出 | 成立 | 两个 `cuexis_shared_export_surface` 均通过；dumpbin 直接列出两个符号 |
| 默认安装面不含实验头 | 成立（源码检查） | 未把实验头加入默认 `FILE_SET`；未做安装消费者复测（T-04 已覆盖安装面） |

总体结论：**部分成立**。几何、选择、投影和 GL 借用路径的主要行为成立；`framebuffer_incomplete` 子项无法在本机驱动上构造，T-06 的 materializer 接线不在本卡范围。

## 预设与理由

选用 `debug`：卡的任务级验收明确要求先配置 `debug` 并构建 `cuexis_render_opengl_tests`；该预设带 SDL/OpenGL 适配器，能建立真实 GL 上下文，同时保留 `SDL_VIDEODRIVER=dummy` 的 CTest 环境供测试内部覆盖。

另用 `headless-debug` 验证纯几何实现不依赖 GL 适配器；用 `shared-debug` 与 `candidate-shared-debug` 验证两种共享 flavor 的逐符号导出。均复用 `out/build` 下已有产物，未做全量 clean build。

## 结果与证据：实现与验收

### 1. 几何与选择（R5-01/03/04/06/09）

在 `engine/playback/experimental_include/cuexis/playback/experimental/frame_layout.hpp` 定义合同值类型与两个导出函数；`engine/playback/src/frame_layout.cpp` 实现：

- 比例域为 1..2147483647；rotation 只接受 0/90/180/270；host 边界用 `uint64_t` 校验；空 target/host 或取整为 0 返回成功空值。
- 非空 host `(W,H)` 与显示比 `(a,b)`：`W*b <= H*a` 时 `w=W, h=floor(W*b/a)`，否则 `w=floor(H*a/b), h=H`；`content=(floor((W-w)/2), floor((H-h)/2), w, h)`，余数留右/下。
- 90°/270° 交换显示比；Exact 仍用未旋转作者 pair。
- Default 选首项；BestFit 以旋转后的实际像素面积 `uint64_t(w)*h` 最大化、平手取最早；Exact 用约分后相等匹配；空表 Default/BestFit 走无比例路径，Exact 空表报 `aspect_unavailable`；请求非法报 `invalid_selection`。
- 纯布局表接受接受域内的未约分 pair，但按约分比拒绝重复；未约分拒绝属于 owning prepare（T-06）职责，与 R5-06 的“内存 owning prepare 用未约分 42:18 拒绝”一致。

实测向量（均来自上述测试）：

| 向量 | 实测 |
| --- | --- |
| 21:9，1280×720 | content `(0,86,1280,548)` |
| 21:9，1280×721 | content `(0,86,1280,548)` |
| 9:16，720×720 | content `(157,0,405,720)` |
| 21:9，host `(100,50,1280,721)`，extent 1600×1000 | content 相对 `(0,86,1280,548)`；绝对 `(100,136,1280,548)`；GL `(100,316,1280,548)`；host GL `(100,229,1280,721)` |
| 21:9，实际 drawable 1660×718 | content `(0,3,1660,711)` |
| 2147483647:1，extent 2147483647×1 | 满区成功 |
| 2147483648:1 | `playback.presentation.layout.invalid_aspect` |
| host.x=UINT32_MAX,width=1，extent.width=UINT32_MAX | `playback.presentation.layout.invalid_target` |
| 空 host、空取整 | 成功空值，不绘制 |
| BestFit 100×100，`[(2:1),(1:2),(16:9)]` | 16:9，`(0,22,100,56)` |
| BestFit 100×100，前两项 | 2:1，`(0,25,100,50)`；平手取首项 |
| BestFit 200×100，90° | 1:2 |
| Exact 4:2 | 匹配 2:1 |
| Exact 3:2 | `playback.presentation.layout.aspect_unavailable` |
| 空表 BestFit/Exact | BestFit 无比例满区；Exact 拒绝 |

命令与证据：

```
ctest --test-dir out/build/debug --no-tests=error -N -R "^rr_frame_layout$"
  exit 0；仅注册 rr_frame_layout（ctest-N-exact.log）

ctest --preset debug --no-tests=error -R "^rr_frame_layout$"
  exit 0；1/1 passed（ctest-preset-debug-exact.log）

ctest --test-dir out/build/debug --no-tests=error -N -R "^rr_frame_layout"
  exit 0；列出 rr_frame_layout 及三个纯值子用例（ctest-N.log）

ctest --test-dir out/build/debug --no-tests=error -R "^rr_frame_layout" --output-on-failure
  exit 0；4/4 passed（ctest-rr_frame_layout.log）
```

### 2. 投影组合与逆算（R5-10）

`engine/playback/src/frame_layout_internal.hpp` 的 inline `detail::composeFrameProjection`：

- 旧谱 0° 直接返回基础 P；90°/180°/270° 为 `R*P`（旧谱 Q=I），其中 90° 的 reciprocal aspect 由调用方在基础 P 中处理。
- 有作者画幅时：旋转后显示比 r；`k=max(w/r,h)`；`Q=diag(k*r/w,k/h,1,1)`；输出唯一 `Pout=Q*R*P`。
- 非有限输入、零 content、未知 rotation、不可表示/退化的 Q 均返回 `playback.snapshot.camera_invalid`。

实测：

- 21:9，host 1280×721：`Pout(0,0)=P(0,0)`，`Pout(1,1)=P(1,1)*(1280*9/21)/548`。
- 21:9，host 721×1280，90°：content `(86,0,548,1280)`；`Qxx=960/959`，`Qyy=1`；作者 NDC `(1/4,-1/2)` 映到 content 像素 `(958/7,800)`、host 像素 `(1560/7,800)`。
- 四旋转（2:1，host 200×100 / 100×200）：作者 NDC `(0.5,0)` 分别映到 `(150,50)`、`(50,50)`、`(50,150)`、`(50,50)`；仅交换 aspect 的阴性到 `(75,100)`。
- 90° 时 `Pout(1,1)=0`，`Pout(1,0)=-P(0,0)`，不能把 `Pout.yy` 当竖向透视系数；基础 P 在 90°→2*atan(0.5) 时 `Pyy` 为 1→2，scale=2 时为 0.5→1，基础 P 与最终矩阵条目分开断言。
- 完整 `(Pout*V)^-1` 一次反算作者 NDC 误差在 1e-6 内；先逆 Q/R 再逆 Pout 的阴性路径明显偏离。

命令：

```
out/build/debug/bin/cuexis_render_opengl_tests.exe rr_frame_layout -s
  exit 0；608 assertions in 1 test case；含 R5-10。
```

本卡只声明纯值向量通过；materializer 把 Pout 写入快照属于 T-06，未在本卡执行。

### 3. 借用 FBO 与 GL 状态（R5-02、R5-11）

`engine/render_opengl/src/open_gl_frame_layout.cpp` 实现：

- `validateBorrowedFramebuffer`：只接受单样本、非 layered 的 renderbuffer 附件；color0 必须为 `GL_RGBA8`，depth 必须为 `GL_DEPTH_COMPONENT24` 或 `GL_DEPTH_COMPONENT32F`，两者实际尺寸等于 extent；拒绝 texture 附件、其它颜色附件、stencil/packed depth-stencil、多样本、尺寸/格式不符和不完整 FBO。错误码 `render.opengl.presentation.target_mismatch`，`reason=attachment_layout|framebuffer_incomplete`，带实际附件/格式/尺寸。
- `drawBorrowedFrame`：保存并恢复 read/draw FBO、全部 `GL_DRAW_BUFFERi` 路由、renderbuffer、viewport/scissor、clear 值、color/depth mask、depth/blend/cull/stencil/rasterizer/logic/sRGB、polygon mode、depth range、program/VAO/array buffer、UBO（含 slot 0）和活动 texture 及前 8 个 texture unit 的 2D 绑定。
- 顺序：验证附件 → 显式 `glDrawBuffer(GL_COLOR_ATTACHMENT0)` → host scissor 清 color+depth → content viewport+scissor 绘制 → 检查 GL error → 恢复。空 content 不调用 GL。失败路径（验证失败或 draw 回调失败）同样恢复路由和全部触及状态。

真实上下文与实测：

- 测试内部清除 `SDL_VIDEODRIVER=dummy` 后建立真实上下文：`3.3.0 NVIDIA 616.64 / NVIDIA GeForce RTX 4060 Laptop GPU`（direct verbose log 第 1285 行附近）。
- 合法 FBO：1600×1000，color0 `GL_RGBA8` + depth `GL_DEPTH_COMPONENT24`，初始所有 draw buffer 为 `GL_NONE` 且 complete。host GL `(100,229,1280,721)`，content GL `(100,316,1280,548)`。
- 绘制后：content 中心像素 `(255,0,255,255)`；host 留边 `(51,102,153,255)`（clear 色 0.2/0.4/0.6）；host 外 sentinel `(0,255,0,255)` 不变；全部 draw route 恢复 `GL_NONE`；预设的 viewport/scissor/clear/mask/blend/cull/stencil/program/VAO/buffer/UBO/texture/FBO/renderbuffer 状态全部恢复。
- draw 回调返回错误时：`drawBorrowedFrame` 返回注入错误，状态和路由仍恢复。
- 负例 color1-only、缺 depth、texture 附件、depth-stencil、4× 多样本（经单样本 FBO blit 读回）、一附件尺寸不符：均在 clear/draw 前返回 `target_mismatch`，`reason=attachment_layout`，像素保持 green sentinel。多样本 FBO 不能直接 `glReadPixels`，测试用 `glBlitFramebuffer` 解析后读回。
- 不完整 FBO：本机驱动对“draw buffer 指向缺失 color5”仍返回 complete；对“read buffer 指向缺失 color1”也返回 complete；其它可构造的不完整布局都会先被附件校验归入 `attachment_layout`。因此 `framebuffer_incomplete` 分支未被触发，测试以 `WARN` 记录（verbose log 第 3150 行附近）。缺什么：需要一个在附件校验通过后仍返回非 complete 的驱动/构造；这属于驱动或合同边界缺口，不能以跳过或纯值测试替代像素验收。

命令：

```
ctest --preset debug --no-tests=error -R "^rr_frame_layout$"
  exit 0；1/1 passed（ctest-preset-debug-exact.log）
```

### 4. 构建、注册、导出与安装边界

- `cmake --preset debug`：exit 0。
- `cmake --build --preset debug --target cuexis_render_opengl_tests cuexis_playback_tests`：exit 0。
- `ctest -N -R "^rr_frame_layout$"`：exit 0，精确注册 `rr_frame_layout`；前缀 `^rr_frame_layout` 另列出三个纯值子用例（CTest 编号会随 discovery 漂移，不作为名字）。
- `ctest --preset debug -R "^rr_frame_layout$"`：exit 0，1/1 passed；前缀执行 4/4 passed。
- 既有 OpenGL 测试套件 `ctest -R "OpenGL"`：exit 0，21/21 passed，未发现目标接线回归。
- `headless-debug` 构建 `cuexis_playback_tests` 并运行：exit 0，3/3 passed；纯几何不依赖 OpenGL 适配器。
- `shared-debug` 与 `candidate-shared-debug`：两个 `cuexis_shared_export_surface` 均 exit 0；dumpbin 明确列出 `?computeContentRect@experimental@playback@cuexis@@...` 与 `?computeFrameLayout@experimental@playback@cuexis@@...`。
- 默认安装面：实验头只在 `experimental_include` 下，未加入默认 `FILE_SET`；playback/render 目标用 PRIVATE include 根，不向普通源码消费者传播。默认安装消费者复测属于 T-04 的安装面，未在本卡重复。
- `python -B tools/verification/check_source_naming.py`：exit 0；`check_header_self_containment.py` 在 vcvars 下：exit 0（headers=27 failures=0）；八份新文件 `clang-format --dry-run --Werror`：exit 0。

## 限制与未做

- `framebuffer_incomplete`：无法核实。驱动在附件校验通过后不产生非 complete 状态；本机可构造的缺附件、color1-only、texture、depth-stencil、多样本和尺寸不符都在更早的 `attachment_layout` 阶段返回。T-06 或设备/驱动 lane 需要用能产生该状态的构造复验，或由合同明确该分支的触发条件。
- materializer 写快照、`submitLaidOut` 公共桥、`LaidOutFrame`、`preparedFramings` 与 `extractLaidOutFrame`：未执行，属于 T-06。
- 实体键盘、音频输出、听音：本卡不涉及；本卡唯一“设备”是真实 GL 上下文，已由 NVIDIA 3.3 驱动实际执行。
- 默认安装前缀与外部消费者：未在本卡重复；T-04 的安装闭包仍是该面的证据。

## 改动文件

新增：

- `engine/playback/experimental_include/cuexis/playback/experimental/frame_layout.hpp`
- `engine/playback/src/frame_layout.cpp`
- `engine/playback/src/frame_layout_internal.hpp`
- `engine/render_opengl/src/open_gl_frame_layout.cpp`
- `engine/render_opengl/src/open_gl_frame_layout_internal.hpp`
- `tests/playback/frame_layout_tests.cpp`
- `tests/playback/frame_layout_vectors.hpp`
- `tests/render/frame_layout_tests.cpp`

修改：

- `engine/playback/CMakeLists.txt`：新增源与 PRIVATE include 根。
- `engine/render_opengl/CMakeLists.txt`：新增 GL helper 源与 PRIVATE include 根。
- `tests/playback/CMakeLists.txt`：注册 `frame_layout_tests.cpp` 与实验 include 根（同文件另有并行 T-17 的改动，不属于本卡）。
- `tests/render/CMakeLists.txt`：注册 `frame_layout_tests.cpp`，测试 PRIVATE 链接 Playback/Platform 并加 include 根。
- 根 `CMakeLists.txt`：`cuexis_render_opengl_tests` 的具名 `ALLOWED` 增加 `cuexis::playback`、`cuexis::platform_sdl`。
- `cmake/VerifySharedExports.cmake`：必需符号、公开函数白名单和 experimental 命名空间白名单增加两个函数。

未修改 `docs/CURRENT_STATUS.md`、`docs/HANDOFF.md`、`docs/stage_plans/active/stage-07-rr/{plan.md,findings.md,schedule.md}`，未修改其它任务的 `out/review` 证据，未 git add/commit。

## 证据索引

全部位于 `out/review/stage-07-rr/T-05/`：

- `report.md`：本报告。
- `commands.log`：命令、退出码、日志映射。
- `run-evidence.ps1`：从工作树根复现本卡证据的脚本。
- `configure-debug.log`、`build-debug.log`、`ctest-N.log`、`ctest-N-exact.log`、`ctest-preset-debug-exact.log`、`ctest-rr_frame_layout.log`、`rr_frame_layout-verbose.log`。
- `ctest-opengl-suite.log`、`build-headless.log`、`ctest-N-headless.log`、`headless-tests.log`。
- `build-candidate-shared.log`、`ctest-shared-candidate.log`、`build-shared.log`、`ctest-shared-default.log`、`dumpbin-shared*.txt`、`dumpbin-candidate-shared*.txt`。
- `naming-check.log`、`header-self-containment.log`、`format-check.log`、`git-diff-check.log`、`git-status.txt`。

## 建议主线回写

- T-05 的 finding/状态可写：几何与选择（R5-01/03/04/06/09）成立；Q/R/Pout 与完整逆算（R5-10）纯值成立；借用 FBO 的 color0 路由、状态恢复、draw failure 恢复和五类附件负例成立；`framebuffer_incomplete` 在本机驱动上无法构造，留待 T-06 或设备 lane 复验。
- T-06 接续同一物理头与 `detail::composeFrameProjection`；由 T-06 补 `LaidOutFrame`、`preparedFramings`、`extractLaidOutFrame`、公共 GL 桥与实验 `FILE_SET`。
- 合同可考虑明确 `framebuffer_incomplete` 的可达条件，或把它写成防御性分支并说明附件校验优先；避免后续测试重复遇到“驱动报 complete”的不可达路径。
- 证据是在与 T-02/T-17 并行的混合工作树上取得的；主线合并前如要求单任务整洁树，可在只包含本卡改动的检出处重跑 `run-evidence.ps1`。
