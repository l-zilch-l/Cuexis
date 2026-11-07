# Example Index

状态：现行示例索引

更新日期：2026-08-10

示例文件用于评审、验证和回归，不自动代表生产 Loader 支持。

- [Stage Chart Format Update candidates](chart_format_update/README.md)：CXC、Chart v4、CXT
  的合法/非法候选 JSON 和 CXT 文件。
- [demo20s 20 秒旋转逼近示例](demo20s.md)：一个可由 `cuexis_player --project` 运行、贴图绕竖直轴
  旋转 7200° 并向摄像机逼近的 Source Project 示例。

候选示例的解析、CXC entry 顺序、CXT import 和运行时脚本拒绝边界由
[tools/check_docs.py](../../tools/check_docs.py) 检查。
