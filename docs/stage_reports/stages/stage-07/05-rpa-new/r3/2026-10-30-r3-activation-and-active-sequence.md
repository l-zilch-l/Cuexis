# R3 activation 恢复点与活跃索引共享实验

状态：隔离候选未选用；继续独立验证活跃索引的产品改动

日期：2026-10-30

返回 [RPA 报告索引](../README.md)。当前状态只见
[CURRENT_STATUS](../../../../../CURRENT_STATUS.md)。

## 机制与正确性

承接[发布归因](2026-10-30-r3-publication-attribution.md)，v5保留原 Fact≥512 触发，新增
同Tick实际 activation≥512 的触发。计数来自 timers 前后 activeRequirements 大小差，
不按数值0 Tick/H5写特例，不把大量无效 windowOpen/deadline 当成重工作。
原字节计费、advance成功后提升、source-floor、可选失败回退规则不变。

v5相关四套件431 cases通过。511/512/1024阈值、activation frontier前后、Snapshot精确值、
后续继续执行的Replay字节、禁用预算回退通过。禁用预算测试仅证明关闭后回退，不冒充计费后拒绝。
独立真分配失败观察在lease acquire后失败，验证退款；最终提交前失败时暂存点未提升。
成功的fault组37 assertions、内存组50 assertions通过。

v6进一步将私有RuntimeState.activeRequirements从vector改为已有SharedSequence<size_t>，
避免每次恢复/无activation Tick复制整份数字数组；DTO仍vector<uint64_t>，wire不变。
新增activation若破坏规范顺序，先物化、排序、构造新序列，成功后才替换；普通读取只共享页。
相关四套件431 cases通过。补充显式63/64/65个高索引先激活、索引0后激活，检查DTO规范顺序、
decode/recover接受及旧归档原值；补充RPA套件82 cases /28256 assertions通过。

组合的惰性Fact缓冲只在剩余≥64时构造，较小前缀沿用单条读取。真实129phase+receipt、
63/64和127/128、非零起点、跨批mutation、Runtime失败重试，以及第二批非法phase失败后
不安装任何投影、修复后完整重试，三cases /2016 assertions通过，均覆盖bindings有/无。
只读审查未发现实现阻塞；普通组仍独立决定取舍。

## 普通固定组

每版Seek固定24进程、四目标各三对AB/BA/AB，每进程原三请求。baseline均为v4 kernel +
原单条发布。未改旧组、未补样；所有目标Snapshot raw全字节/hash一致。

| 候选 | 40K H5 baseline→candidate | 40K H902 baseline→candidate | 数值/保护门 | 全组重复性 |
| --- | --- | --- | --- | --- |
| v5 activation | 16.420→0.367 ms | 2.330→2.987 ms | H902退化28.20%，失败 | 未过 |
| activation+惰性批量 | 16.272→0.445 ms | 2.125→1.559 ms | 通过 | H5候选22.70%、final基线20.63%，未过 |
| v6再加活跃索引共享 | 16.449→0.355 ms | 2.184→1.517 ms | 通过 | 三个baseline超15%，未过 |

v6四个candidate的进程中位数相对范围分别8.37%/11.27%/3.96%/13.25%，但没有据此改判整组。
所有候选请求≤20ms，仅CPU测试呈现，不是物理GPU、正式配对CI或全部适用目标接受。

v6 writer另固定18进程，1K密集/40K密集/1536六波各三对，双方都启用各版capture。
advance中位数分别+3.37%/-2.42%/-1.81%，保护门通过，1K双方重复性超15%，整组未通过。
首个writer组因旧探针错误地要求密集场景1点，而新策略实际2点，exit42停止；原组保留。
新组仅修正测试点数预期，无源码替样。

## 实际保留与限制

现有40K Tap单条长记录夹具，writer销毁但archive仍保留时，v5比禁用capture多保留
12225100B requested、15785644B CRT storage；v6为11585022B /15145502B。
v6 payload保守计费32492616B、当前两点。两版最终owner释放后requested/storage差值均0；
全程requested峰值约171–172MB，含测试观察器，不能作为完整播放链或生产cap接受。
这是Tap夹具，不能套给此前三phase Hold的约43MB phase计费。

v3 observer误用旧revision3 Packed夹具导致40K字符串额度拒绝，保留失败；v4改为已有revision4
路径，没有放宽额度。所有观察、源码补丁、组登记、失败说明及身份补充见
[manifest](2026-10-30-activation-evidence/manifest.json)。旧build receipt继承的v4/v5文案
已由identity-supplement补充实际链接对象，不追改冻结文件。

128MiB仍只是实验参数；客户端任意持有旧archive不受四槽约束。全链headroom、较长历史、
一般上界、全部cold/external/fallback和最终工具链矩阵继续开放。上述v5/v6均selection=false，
下一步仅单独检验不依赖R3c/预算/批量发布的活跃索引共享，避免把隔离原型的成绩写入产品。
