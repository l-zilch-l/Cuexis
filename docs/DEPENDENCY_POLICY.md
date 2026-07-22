# Cuexis 开源与第三方依赖政策

状态：已接受

更新日期：2026-07-20

## 项目许可

Cuexis 采用 Apache License 2.0。维护者不以商业盈利为目标，但许可证不限制下游的商业使用；再分发者必须遵守 Apache-2.0 的版权、许可证、NOTICE 和修改声明要求。

正式公开发布前，根目录必须包含从 Apache 官方来源取得的完整 `LICENSE` 文本。若项目包含需要 NOTICE 的内容，根目录同时维护 `NOTICE`。

## 依赖选择

通用系统优先采用成熟、持续维护的开源库。评估顺序：

```text
功能与平台适配
维护状态和安全响应
许可证兼容性
vcpkg / CMake 集成
测试和文档质量
构建时间与二进制体积
公共 API 泄漏程度
替换和退出成本
```

MIT、BSD、Apache-2.0 和 zlib 等宽松许可证通常可以接受。MPL、LGPL 和 GPL 依赖必须逐项审查其链接、修改公开和再分发义务；未经 ADR 和明确合规方案不得进入正式发布依赖。

## 依赖记录

每个直接依赖至少记录：

```text
名称和上游 URL
固定版本或 vcpkg baseline
用途和所属模块
许可证与 NOTICE 要求
是否进入分发产物
替代方案和退出路径
```

发布产物维护 `THIRD_PARTY_NOTICES`，列出直接依赖以及依法需要披露的传递依赖。不得仅依赖 vcpkg 缓存作为许可证记录。

## 封装原则

第三方库可以用于内部实现，但除明确基础类型外，不进入 Cuexis 公共接口。Chart、Component 和资产格式不得保存第三方运行时对象。后端库通过模块边界封装，替换依赖不应要求修改无关模块。

cuexis_playback 与 cuexis_judgement 的安装公共头使用更严格的封装规则：不得要求消费者包含 EnTT、SDL、OpenGL/GLAD、JSON 实现或日志实现头。可选后端依赖只能由对应 CMake component 传播，纯 `Cuexis::Playback`/headless consumer 不得被迫安装 SDL 或 OpenGL 依赖。

## SDK 分发

每个正式分发组件必须记录其公共与私有传递依赖、静态/动态链接方式、许可证文件安装位置和消费方义务。`CuexisConfig.cmake` 的组件依赖必须与实际链接边界一致；不能通过 umbrella target 静默把 Player、Studio、测试或后端依赖带入宿主。

官方宿主适配器可以依赖对应引擎 SDK，但该依赖不得进入 Playback 核心。无法随 Cuexis 再分发的宿主 SDK 必须采用由消费者提供的查找方式，并在构建和许可证文档中明确说明。

## 例外流程

引入大型依赖或具有传播性许可证的依赖时必须写 ADR，说明不用成熟库、自研以及其他候选的总成本。项目免费不构成忽略许可证义务的理由。
