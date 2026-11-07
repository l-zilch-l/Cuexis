# ADR 0028：相机投影模型、ECS 组件归属与 FrameSnapshot 契约

日期：2026-07-20

状态：已接受

## 背景

Cuexis 谱面需要可配置的相机视角来定义播放期间的观察方式。相机的初始姿态（位置、角度、FOV）应在谱面数据中声明，而播放期间的相机事件（位置移动、旋转、FOV 变化）应通过 Behavior 驱动。阶段 1C 使用 Behavior Keyframe，Chart v3 使用 Behavior Event。同时，相机数据需要在 SDK 的 FrameSnapshot 中传递，使宿主能够以独立于渲染后端的方式获取相机参数。

## 决策

### 1. 模块归属

`CameraComponent` 归属 `cuexis_render`，不归属 `cuexis_world`。理由：相机的投影参数（FOV、裁剪面）是渲染域概念，非通用空间变换。相机的位置和旋转使用 `cuexis_world` 中的 `TransformComponent` 和 `WorldTransformComponent`。

### 2. 谱面表示

相机在谱面中有两层表示：

**顶层默认配置**（`camera` 字段，可选）：
```json
{
  "type": "perspective",
  "fovY": 45.0,
  "near": 0.1,
  "far": 1000.0,
  "pitch": -15.0,
  "yaw": 0.0,
  "roll": 0.0,
  "defaultTransform": {
    "position": [0.0, 5.0, -10.0]
  }
}
```

**对象相机组件**（`cuexis.camera` Component，可选）：任何 Object 可以通过挂载 `cuexis.camera` 组件成为相机实体。多个相机实体可以共存；第一个标记为相机的实体作为活动相机。

```json
{
  "id": "...",
  "components": {
    "cuexis.transform": { "version": 1, ... },
    "cuexis.camera": {
      "version": 1,
      "type": "perspective",
      "fovY": 45.0,
      "near": 0.1,
      "far": 1000.0
    },
    "cuexis.behavior": {
      "version": 1,
      "behavior": {"domain": "behavior", "id": "behavior.camera.move"}
    }
  }
}
```

### 3. ECS 组件结构

```cpp
// engine/render/include/cuexis/render/camera_component.hpp
namespace cuexis::render {

struct CameraComponent final {
    std::string type{"perspective"};
    double fovY{60.0};
    double nearPlane{0.1};
    double farPlane{1000.0};
    double pitch{0.0};
    double yaw{0.0};
    double roll{0.0};
    core::Mat4 projectionMatrix{};
};

}
```

### 4. 投影矩阵计算

投影矩阵在运行时计算，使用 `core::makePerspective(fovYRadians, aspectRatio, nearPlane, farPlane)`。`aspectRatio` 由宿主视口尺寸提供，**不**存入谱面——这避免谱面绑定特定分辨率。Pitch/Yaw/Roll 按 Tait–Bryan (ZYX) 顺序：Roll → Pitch → Yaw，转换为四元数旋转后置入 `TransformComponent.rotation`。

### 5. FrameSnapshot 契约

`FrameSnapshot::CameraSnapshot` 包含以下字段，全部为后端无关类型：

| 字段 | 类型 | 说明 |
|---|---|---|
| `active` | `bool` | 是否有相机实体 |
| `fovY` | `double` | 垂直视场角（度） |
| `nearPlane` | `double` | 近裁剪面 |
| `farPlane` | `double` | 远裁剪面 |
| `pitch` | `double` | 俯仰角（度） |
| `yaw` | `double` | 偏航角（度） |
| `roll` | `double` | 翻滚角（度） |
| `projectionMatrix[16]` | `float[]` | 列主序透视投影矩阵 |
| `viewMatrix[16]` | `float[]` | 列主序世界→视图矩阵 |

### 6. Behavior 事件驱动

相机对象可通过 `cuexis.behavior` 绑定 Behavior。阶段 1C 使用 `behavior.transform.keyframe` v1；Chart v3 使用 `behavior.event`，两者都驱动以下属性：

| 属性路径 | 说明 |
|---|---|
| `transform.position.x/y/z` | 相机世界坐标 |
| `transform.rotation.*` | 相机四元数旋转 |
| `camera.fovY` | FOV 动画 |

### 7. 宿主契约

宿主提供视口尺寸（width/height）。Cuexis 根据视口计算 `aspectRatio` 并构建投影矩阵。宿主从 `FrameSnapshot.camera` 获取完整的投影+视图矩阵用于自己的渲染管线。

## 备选方案

### 相机完全作为顶层配置（拒绝）

相机仅作为顶层 `camera` 字段——不支持对象相机、多相机或 Behavior 事件驱动。

拒绝理由：无法通过 Behavior 驱动相机事件，无法支持多相机或相机切换。

### 相机对象使用专用相机实体类型（拒绝）

在 `objects` 之外建立独立的 `cameras` 数组。

拒绝理由：破坏统一的 Object/Component 模式。Cuexis 的核心理念是所有语义实体都是统一 Object 通过 Component 组合表达。

### 相机归属 `cuexis_world`（拒绝）

将 `CameraComponent` 放在 `cuexis_world`。

拒绝理由：投影参数属于渲染域而非通用空间域。World 模块应只包含空间相关组件（Transform、Hierarchy）。

## 影响

- `cuexis_render` 新增 `CameraComponent`
- `cuexis_playback` 的 `FrameSnapshot` 新增 `CameraSnapshot`
- `cuexis_chart` 的 `ChartDocument` 新增 `CameraData`，`ObjectComponents` 新增 `cameta`
- `cuexis_runtime` 的 `ChartWorldInstantiator` 新增相机实体创建
- `cuexis_core` 新增 `makePerspective()`
- Chart JSON Schema 新增顶层 `camera` 属性和组件 `cuexis.camera`
- Chart 格式文档新增 §4a 和 §8a
- 153/153 现有测试通过，stage1b fixture 新增相机对象

## 后续风险

- 多相机选择/切换策略尚未冻结（当前取"第一个相机对象"）
- 正交投影未实现，保留为 `type` 枚举的扩展点
- FOV 动画依赖 BehaviorSystem；v1 使用 Keyframe，v3 使用 Behavior Event
- 相机 Shader 参数传递路径（CameraComponent → uniform）将在阶段 3/5 细化
