# RPA B0 合同修订与决策记录

状态：active；合同修订记录，B0未退出，未实施产品

更新日期：2026-10-11

范围、批次与准入归[RPA分册](../../../../stage_plans/active/stage-07/plan-rpa.md)；
方向裁定归[生产架构方案](2026-10-11-production-architecture-design.md)。
本记录按讨论轮次保留当时已写合同、未闭合细节和候选方案，不新增实施授权。
§1–9中的“本次/仍待”指各自历史轮次，最新委托裁定收口见§10；不得据较早段落重新打开已闭合设计。
本轮用户要求：遇到需要owner决定的事项，至少列出四个不同方案并停止等待选择。

## 1. 本轮证据边界

基线：分支`stage-7`，HEAD=`7ca1fc6cf2dd4c7072aab3ae260ee5931f5df2d3`，
首次修订前工作树有30个未提交条目；O1裁定后的续修继续基于同一分支/HEAD及已有文档修改，
HEAD不是完整工作树身份。本轮保留既有源码/测试修改。
以下为文档与现有测试入口的只读核对；未执行C++构建、CTest、性能测量、设备或hosted验收。
历史验收不因本次合同编辑获得新的适用范围。

## 2. D-TB01 已写入合同

落点：[V2 Spec §3.7.3](../../../../formats/GAMEPLAY_V2_SPEC.md#373-observationtick-的唯一来源与-s7c-1-边界q-04--cm-t04)。
本项是补充解释，**没有被替换条款**。保留的原文是：

> `observationTick` 的**唯一 canonical 来源**是**输入进入判定管线时捕获的校准后会话时钟
> （calibrated session clock）**。

新增解释：主线程被present阻塞期间的按键，取恢复后实际捕获时的当前Tick；
交付执行域时不重新采样，不按物理按键时刻回溯，也不按帧数/按键数合成Tick。

最小反例与确定预期：假设校准时钟在阻塞前为100，阻塞期间物理按键发生于150，
恢复并实际捕获时为200，则`observationTick=200`，不是150；若交付时为230，也不改为230。
这些数值只说明语义，不是生产时延预算；准入结果由交付时的既有late-policy决定。

| 影响面 | 本次结论 |
| --- | --- |
| identity | 不新增字段或改变结构key；捕获所得Tick仍参与原有canonical subject。不同捕获时刻可能生成不同subject，不能承诺相同物理操作必得相同结果 |
| Replay | 重放已记录的canonical输入和admission轨迹，不在重放时重新读取时钟；不改变archive或Recovery格式 |
| 错误 | 沿用时钟回退、不可表示Tick及迟到输入的现有拒绝路径；本解释不新增诊断码 |
| 状态转换 | 不新增会话状态，不改变Paused入口限制或封存边界 |
| ABI/API/profile | 不新增公共符号；现有唯一来源与准入合同继续适用，字段布局不变 |

受影响测试入口（**待后续授权实施/运行**）：

- `tests/judgement/observation_normalization_tests.cpp`：捕获先于规范化、回退/溢出拒绝保持。
- `tests/player/player_gameplay_tests.cpp`：增加阻塞后捕获与延迟交付场景；后续B3替换生产`hStep`时，
  不得把现有逐转换时钟夹具当作capture-time的实时证明。
- `tests/judgement/execution_kernel_tests.cpp`：相同canonical/admission轨迹下完整结果仍可比较；
  本次不宣称不同交付轨迹结果相同。

## 3. D-TB03 输入顺序：O1已裁定并落约

owner于2026-10-11回复“按照你推荐的O1”。选择来源分组连续处理，O2/O3/O4不采用。
完整输入处理合同已写入[execution profile §3.4](../../../../formats/gameplay-v2-execution-profile.md#34-rpa-t2-输入批处理顺序o1已裁定未实施)，
并同步V2 Spec §3.7.2与S7A-2禁止清单。以下保留裁定时的问题和四方案比较作为决策依据；
“当时未定义”不是当前输入顺序仍待裁的声明。

T2的支持范围继续有效：一个Tick一个已关闭输入批、快速点按与和弦，
不引入同Tick多timer与输入交错。本项决定的是**输入批内部的处理顺序**，不重新选择T1–T4。

现行[execution profile §4.1–4.2](../../../../formats/gameplay-v2-execution-profile.md)定义：

> CanonicalObservationKey=`(observationTick,domainToken,sourceClass,channelToken,actionRank,
> amountPresence,amountInteger)`；action rank press=0,release=1,update=2。

同一来源已按住时，合法转换可以是release→press。按上述完整key直接排序会变成press→release；
按该来源单调ingress sequence则保留release→press。裁定前方案已选择后者解释同来源序列，
但当时未说明多个来源的序列怎样合并。不同合并方式会改变共享资源可用时点和竞争结果。

O1中的“来源”分组沿用现有contact通道结构
`(domainToken,sourceClass,channelToken)`，不是整个SDL设备，也不是到达线程。
下列四种候选都保留每组内部已知先后，只用结构键决定跨组平局，不以全局到达次序裁决。
它们仅在输入阶段内运作，不改变timer阶段或直接定义Fact总序。

| 候选 | 确定的输入处理规则 | 维护成本与行为代价 |
| --- | --- | --- |
| T2-O1 来源分组连续处理（已选择） | 按来源结构键排序各组；每组按本组sequence处理完，再处理下一组 | 最简单，最接近原key先比较来源的次序；较小来源可连续完成松开/再按，不保证其他来源抢到中间释放机会 |
| T2-O2 逐轮交织 | 第一轮每组取第一个转换，第二轮每组取第二个，依次处理；每轮按来源结构键排序 | 增加组内位置这一语义；其他来源能插入松开与再按之间，来源较长时不会一次处理到底，但不等于资源公平保证 |
| T2-O3 可执行的release优先 | 每组仅暴露尚未处理的第一个转换；优先选择其中的release，其余按完整canonical key排序；每处理一次重选 | 倾向先释放旧占用；需要反复选择，改变跨来源action优先级；不能跳过本组更早的press来提前release |
| T2-O4 可执行的press优先 | 每组仅暴露尚未处理的第一个转换；优先选择其中的press，其余按完整canonical key排序；每处理一次重选 | 倾向先检查当前占用下的新按键；与O3有相反的交接效果；同样不能逆转本组序列，先失败的输入不因后续release自动重试 |

O3/O4不等于按原完整canonical key排序：它们把指定action的优先级提升到来源键之前。
O1/O2也必须在合同中明示组内sequence的用途；任何选项都不能简单删除“禁止ingress排序”
而开放全局到达顺序。推荐O1的依据是维护性和与现有来源键顺序的一致性，不是实测性能。

### 最小反例与候选预期

约定`D < F`为来源结构键顺序；`+`为press，`-`为release。
所有事件在同Tick的输入阶段；两来源的新Hold都具备其它资格并争用同一非Terminal资源。
旧Hold仍有待完成phase，owner release会结束旧lease；不涉及该Tick的timer交错。

| 场景 | O1 | O2 | O3 | O4 |
| --- | --- | --- | --- | --- |
| 旧owner为D；D序列`D-,D+`，F序列`F+` | `D-,D+,F+`；新D取得资源 | `D-,F+,D+`；新F取得资源 | `D-,D+,F+`；新D取得资源 | `F+,D-,D+`；F先遇旧占用而不能获胜，新D取得资源 |
| 旧owner为F；D序列`D+`，F序列`F-,F+` | `D+,F-,F+`；D先遇旧占用，新F取得资源 | 同O1 | `F-,D+,F+`；新D取得资源 | 同O1 |

同表中部分终态相同不表示语义等价：例如第一行O1与O4的`F+`观察到的owner不同，
considered/receipt轨迹及未来扩展的可解释性也不同。选择只决定合并规则，
不扩大单条观测fanout，也不改写该观测内部已有K4竞争算法。

### 被替换原条款与仍保留的基线

按以下范围逐条处理，不把O1选择当作T2全部合同闭合：

- **后续已由T2-L1替换，原文保留在本记录**，execution profile §3.3：
  “同 Tick 的第二个**不同** canonical subject 仍按 S7A-2 `same_tick_collision` 拒绝”；
  “一个 Tick 可有多个 timer 和至多一个 admitted input；timer 释放后该输入可使用 free slot。”
- **已替换**，V2 Spec §3.7.2原文：“**同 Tick 碰撞按规范键 `(tick, originKind, canonicalOrdinal)` 排序**；
  `canonicalOrdinal` 由 canonical 窗口 / 观测 identity 派生。**禁止按 ingress 顺序排序**，
  也禁止按容器顺序或线程完成顺序排序。”改为区分输入处理与Fact总序，输入顺序引用O1。
- **已替换**，V2 Spec S7A-2不得消费清单：“按 ingress 顺序解决同 Tick 冲突”。
  改为禁止跨来源全局ingress裁决，只开放O1的来源内转换顺序，不开放Fact排序。
- **已补限定说明**，execution profile §4.1的结构key/action rank不覆盖O1输入处理序；
  原identity字段保持，未默认加入sequence。

O1的identity/Replay影响：来源内部真实序列必须可复算，不能靠仅有的action rank恢复；
跨来源全局交付交织不改变输入处理序，但只有相同admission/control轨迹才能比较完整执行结果。
错误与状态影响：O1不新增诊断码或会话状态；身份/关闭批未闭合前不删除现行准入拒绝。
ABI/API影响：不改变公开入口、类型布局或Recovery格式；Fact的ordinal与版本闭包仍是后续工作。

裁定后仍须核对：同一Tick重复press等多次同值观测的identity与去重、关闭批与迟到转发的关系、
原子失败边界、timer阶段与Fact总序。当前sequence只作诊断/admission，不能默认为可直接写入
canonical identity；若Fact总序变化必须处理`factSemanticRevision`，本记录不选择新revision值或格式。

受影响测试入口包括`observation_normalization_tests.cpp`的碰撞/重复校验、
`execution_kernel_tests.cpp`的整批碰撞拒绝、contact/owner control与完整前缀对照、
`execution_reference.cpp`的独立模型，以及Player输入和Replay/Seek回归。
这些是后续影响清单，不是本轮新增测试或通过证据。

## 4. D-TB03 发生身份：I1已裁定并落约

owner于2026-10-11选择“I1”。原始Tick来源序列的局部位置已写入
[execution profile §4.1.1](../../../../formats/gameplay-v2-execution-profile.md#411-rpa-t2-发生身份i1已裁定未实施)，
Spec §3.7.4与ABI说明同步指向该合同。以下保留裁定前问题与候选比较；I2/I3/I4未采用。

### 只读核对与最小反例

现行`CanonicalIngressSubject`只包含Tick、domain/source/channel、action和amount，明确不含
`ingressSequence`；结构见`engine/judgement/include/cuexis/judgement/input_boundary.hpp`。
`engine/judgement/src/input.cpp`的`prepareIngressBatch`先拒绝重复sequence，再比较canonical
subject，后者相同仍拒绝。不同sequence不会使两次同值press获得不同canonical身份。
`tests/judgement/observation_normalization_tests.cpp`已有
“a new ordinal carrying the same canonical observation is refused”的现行拒绝预期。
这些是源码/测试文本核对，本轮没有运行测试。

最小场景：同一个已关闭Tick输入批里，同一来源有真实的`press → release → press`，
两次press的Tick和amount等字段都相同。O1能确定先后，却不能仅凭旧subject区分两次press。
若直接保留旧去重规则，第二次press会被拒；若直接跳过去重，又不能阻止同一事件重复提交。
这里需要决定的是“哪一次发生”如何表示，不改变capture-time、不增加合成Tick、不重新选择O1。

### 四种候选与选择

下表的身份作用域都包含原始observationTick与来源；“现有字段”指现有subject内容。
发生身份在迟到转发前建立，转发不能重新编号；这是候选设计的比较前提，
不预先决定何时关闭批、如何合并转发输入、位宽、溢出码或Recovery字节布局。

| 候选 | 如何区分两次press | 去重/Replay与维护代价 |
| --- | --- | --- |
| T2-I1 Tick内来源局部位置（已选择） | 在已关闭的原始Tick来源序列中按既定先后规范化为0、1、2…；现有字段加局部位置。例中两次press的位置为0与2 | 数值不依赖全局ingress绝对值或其它来源的交织；只需在闭合序列中派生位置。重交必须保持原序列/位置，同位置内容冲突拒绝；不能把重交片段重新编号为新事件 |
| T2-I2 来源全程发生序号 | 为每个来源维护贯穿本次运行的单调发生序号，现有字段加该序号；它不是现有全局ingress数值的别名 | 适合跨批追踪，但增加每来源计数、恢复与耗尽边界；Replay须保留原序号。较早同来源事件的增删会影响后续编号；重交必须复用原发生序号 |
| T2-I3 同值内容出现次数 | 对原始Tick/来源内每一种现有subject分别计第0、1、2…次；例中两个press分别为press#0与press#1 | 不同内容的插入不改变press计数，但计数无法表达press/release的交织；还须保存来源内顺序。比I1多一套逐内容计数与顺序之间的一致性约束；重交保持原计数 |
| T2-I4 整个来源序列作为身份对象 | 原始Tick、来源及完整有序内容序列共同构成规范批对象；单次发生用该对象加位置引用 | Replay天然保留序列与次数，但修改序列任意内容都会改变该对象及其成员身份；需要不可变对象共享与批级去重，不得仅用hash代替结构相等。影响范围最大 |

四种方案都要求明确的稳定重交身份。若生产者把一次重交伪装成新的发生序号、位置或新批，
仅凭同值内容无法证明它是不是物理上的再次发生；不能宣称任选一种就自动识别所有重复设备事件。
完整关闭批/重交协议仍须后续闭合。I1最贴近当前“一个Tick一个已关闭输入批”和O1的局部顺序，
因此按维护性优先推荐I1；这不是已测性能结论，也没有选择任何字段名或字节格式。

### 不同方案的可观察区别

- 原始全局ingress为10、12、15或100、102、105，但同一来源内容/先后相同：I1的局部身份不变；
  I2的新来源发生序号另有保持义务，不是用这些全局值代替。
- 在本Tick之前增加一个同来源事件：I1/I3/I4在本Tick内容不变时不受影响；I2的后续计数会移动。
- 在当前来源序列中某press之前插入不同内容：I1的位置改变；I3该press的同值次数不变；
  I4整序列身份变化。以上只比较身份，不宣称新增输入后判定结果仍相同。

### 本次落约与影响

本次同步了Spec §3.7.4的去重解释、profile §4.1.1的逻辑身份与ContactHandle来源、ABI引用。
没有改变源码、公共API、Recovery格式或revision数值。

| 原条款及原文 | 本次处理 |
| --- | --- |
| Spec §3.7.4：“重复检测的判据是canonical观测identity，不是ingress序号或到达顺序。” | 保留禁止直接使用全局ingress数值，补充I1的来源内派生位置；同值不同位置不再仅因内容相同而重复 |
| profile §4.1：`CanonicalObservationKey=(observationTick,domainToken,sourceClass,channelToken,actionRank,amountPresence,amountInteger)` | 标为现行单输入内容key；T2依§4.1.1补局部位置，旧内容比较不能独自决定T2去重 |
| profile §4.1：“现有ObservationId/IngressSequence只诊断与admission，不能替代该结构key。” | 明确限制的是绝对数值；来源内相对顺序由O1解释、I1派生局部位置，不将旧字段直接改名成canonical序号 |
| profile §4.1：`ContactHandle=(domainToken,sourceClass,channelToken,startPressKey)` | 公式保持；T2的startPressKey指向带局部位置的发生，区别同Tick重新按下后的新contact |

I1不预选整数载体、码值和字节布局。原有“拒绝不消耗已发布状态”和“sealed结果不重开”保持。
Replay必须保留或唯一重建来源序列与位置，不能只保存同值集合；Fact origin/ordinal与版本的
完整影响尚待核对，不能写成旧archive已兼容或新格式已实施。

后续授权实施时需验证：两次press身份不同；同一发生重交仍拒绝；同槽位异内容稳定拒绝且旧状态
保持；全局sequence绝对值变化但来源内顺序/内容不变时I1不变；转发保留原位置；同Tick重按
产生不同startPress身份；完整Replay/Seek重建相同contact与结果。
影响入口包括`observation_normalization_tests.cpp`、`execution_kernel_tests.cpp`、
独立reference模型及`fold_recovery_tests.cpp`；本轮没有新增或运行这些测试。

## 5. 捕获关批：T2-C1已裁定并落约

owner于2026-10-11选择“T2-C1”，合同已写入
[execution profile §3.5](../../../../formats/gameplay-v2-execution-profile.md#35-rpa-t2-捕获关批t2-c1已裁定未实施)。
以下保留裁定前比较；T2-C2/C3/C4未采用。正常Running同段关批证据与责任方已确定，
不等于AD-06推进许可、控制停点和迟到转发合并全部闭合。

### 最小反例与三种不同边界

同一连续有效时钟段内，两次捕获都可能返回`t`：第一次收到D press，第二次收到D release。
若第一次poll后就宣称Tick t完整，第二次是真实新输入，既不是重交也不能改成`t+1`。
I1定义完整序列形成后如何赋予身份，没有授权以一次poll、队列暂空或超时证明完整性。

必须分开：**捕获批关闭**证明原始Tick来源序列完整；**交付前缀完整**证明相关记录已到执行域；
**执行封存**由kernel的advance/admission规则决定。捕获关批本身不推进H或封存Fact。
本选择与AD-06有关，但不自动裁定其frontier载体、时间域、比较谓词或失败状态。

### 四种关批协议与选择

本节使用完整编号T2-C1至T2-C4；T2-C1不是TB-01的capture-time候选C1。

四种候选均保留O1/I1，不丢弃第二次同Tick转换，不制造更大的Tick。
正常Running路径的共同证明前提是：捕获方在所有较早捕获操作完成后，观察到**有效且严格大于t**
的校准时钟，同段后续捕获不得回退到t；覆盖t的完整前缀须先于关批生效。
不必计算`t+1`，设备原始时间或音频帧数不能直接用于比较。
这些共同前提随T2-C1选择写入正常捕获关批合同。

| 候选 | 谁组织完整批、怎样关闭 | 维护成本与行为代价 |
| --- | --- | --- |
| T2-C1 捕获方聚合后主动发布（已选择） | D1按原始Tick暂存捕获记录；有效时钟越过t后，发布覆盖t的完整不可变输入与关批声明；D2校验并派生I1 | 完整性集中在捕获方，D2无需拼未完成片段；D1保留未关闭/未交付数据。等待下一次有效观察会增加交付等待，没有固定等待毫秒数 |
| T2-C2 执行方暂存，捕获方推送前缀证明 | D1及时有序发送记录，再发“时钟越过t且此前记录已覆盖”的声明；D2收齐所指前缀后组批 | 提前交付片段，但增加D2暂存、进度关联与缺失前缀处理；暂存不等于准入。时钟前进不能跳过缺失输入 |
| T2-C3 执行方按需请求关闭屏障 | D2异步请求D1关闭到指定边界；D1取得越界观察并排完相关前缀后回复；D2收到完整回复再组批 | 不持续推送全部关批进度，但增加请求ID、在途/失效回复与取消状态；GPU阻塞时也可能等待主线程。worker不得同步阻塞，超时不代表关批 |
| T2-C4 输入、关批与推进走同一有序命令通道 | D1取得同样的越界证明后，把完整输入、关批和显式advance请求顺序提交；D2按通道顺序消费，H仍取校准时钟 | 减少独立推进与输入通道协调，但普通推进请求也依赖主线程发布；present阻塞会阻塞后续推进请求。须收敛普通advance入口，不能另留绕过关批路径 |

推荐C1依据是维护性：原始Tick完整性在捕获方集中管理，跨域交接完整拥有数据的值。
这不是“主线程停了就自动完整”：未取得越界证据的最后一个Tick仍未关闭，未交付数据仍须保留。
C2暂存、C3请求状态、C4统一命令入口分别带来不同协调成本。四者都不解决present期间捕获停顿，
不能用超时或队列长度把未知前缀标为完整。

时钟冻结、观察失效、Tick最大值、Pause或切段导致正常越界证明不可取得时，不能伪造越界值；
控制停点与剩余批处置仍须明确。本次不预选暂停关批/恢复语义，Paused禁止submit/advance保持。
选择普通关批协议也不自动决定晚到输入的dispatch批合并规则。

### 候选共同预期

两次捕获均为t、第三次有效观察大于t时，前两次同来源记录归入同一原始Tick序列，I1位置为0与1，
不因跨poll变成两个从0开始的批。第三次观察可无输入，但须是有效时钟/前缀证明；队列暂空不够。
关批声明已到、前缀缺失时，C2/C3不能准入完整批；C1/C4不能发布不完整载荷并假称成功。
重交完整批不能把成员作为新发生再次执行；传输回执与错误优先级仍需随所选协议落约。

### 本次落约与影响

原合同未定义原始Tick捕获批的关闭协议，本项为新增合同，不把现有`submitBatch`原子提交
改称捕获关批。替换的是profile §4.1.1结尾“关批证据和发布者仍待裁定”的登记；
同时撤回架构方案AD-06早前“同主线程即可保证无悬空前缀”的无条件推论（上一轮已订正）。

| 影响面 | 本次结论 |
| --- | --- |
| identity | 原始Tick、完整来源序列与I1位置保持；关批证明不自动加入content identity |
| Replay | 完整成员/顺序与准入轨迹须可复算，不把关批时间当作observationTick或commitTick；未选择新编码 |
| 错误与状态 | 已关闭、已交付、已准入、已封存分别解释；无有效越界证据仍未关闭，不能因超时伪成功。未新增公开状态枚举或诊断码 |
| 所有权 | D1保留未关闭/未交付数据；跨域发布完整owning对象，D2不拼未经证明的片段 |
| ABI/API | 本轮只定义逻辑载荷要求，不修改公共符号、源码或Recovery格式 |

后续测试需覆盖：同Tick跨poll、无输入越界观察、时钟相等/失效、不完整发布与发布失败、
重交不重复执行、关批不隐式advance、暂停/切段拒绝假关批。既有Player采样、
`observation_normalization_tests.cpp`和kernel/reference回归是影响入口；新关批载体的测试
在实施获授权后随载体补齐。本轮未写或运行测试。

## 6. 执行批合并：T2-L1已裁定并落约

owner于2026-10-11选择“T2-L1”，并要求完成本裁定修订后停止。合同已写入
[execution profile §3.6](../../../../formats/gameplay-v2-execution-profile.md#36-rpa-t2-执行批合并t2-l1已裁定未实施)。
T2-L2/L3/L4未采用。下文保留裁定前反例与比较，后续影响清单不构成继续工作的授权。

### 现行合同与最小反例

修订前execution profile §3.3的原文为：“转发导致dispatchTick碰撞时也拒绝第二个distinct subject”，
并要求批量碰撞整批失败。`ExecutionKernel::submitBatch`分别检查旧pending与本次entries，
同dispatchTick即返回`judgement.late.dispatch_collision`，其条件还登记在
[execution typed合同](../../../../api/gameplay-v2-execution-types.md)的错误表。
这里的实现结论来自源码阅读，没有新增运行验证；文档检查不证明合并行为已经实现。

现有`execution_kernel_tests.cpp`的“Late dispatch collisions reject whole admission without
poisoning sequences”给出fixture：A=903、C=3、P=900，原始t=900转发到q=901，原始t=901
保持准时q=901。两者同时提交时现行实现拒绝整个准入事务；拒绝不消耗sequence。
这些数字是既有fixture，不是生产参数。

裁定前T2-C1能分别给出完整原始900批与901批，I1也能区分两者；二者未决定能否合入执行901批。
若同来源分别是press和release，是否合并直接影响contact与后续Hold。该反例说明late-policy
还须定义此输入形状，不证明最终T2-C1/AD-06部署必然产生这条轨迹。

### 四种策略与选择

所有方案保留O1/I1/T2-C1，原始Tick与位置不变，不重新开启sealed Tick，不让timer插入输入中间。
方案只讨论通过其它准入校验的完整原始批；重复、冲突、finalized和会改写已seal覆盖的输入继续拒绝。

| 候选 | 规则 | 维护成本与行为代价 |
| --- | --- | --- |
| T2-L1 合并所有合格原始批（已选择） | 保留既有q=P+1路由；同q且未封存的合格成员合并为一个执行输入批。跨来源按O1，来源内按原始Tick再按I1位置连续处理 | 同来源与不同来源统一处理，无新调度Tick；需定义pending批构建与封存，完整Replay记录原始身份/dispatch。较新但来源键较小的输入可先于较旧的其它来源，符合O1而非全局原始时间排序 |
| T2-L2 不允许跨原始Tick合并 | 每个执行Tick只容纳一个原始Tick的完整批；新准入碰到不同原始Tick的已有占位则拒绝，本次事务内部碰撞也原子拒绝 | 最接近旧碰撞拒绝，回归面较小；迟到输入与准时输入相撞仍可能拒绝，不能宣称T2已消除所有碰撞 |
| T2-L3 只合并来源互不重叠的原始批 | 不同原始Tick可以合并，条件是来源键集合互不重叠；同来源跨Tick重叠则原子拒绝。合并后的执行序仍按O1 | 支持不同按键碰撞，避免同来源跨Tick拼接；增加集合相交规则，同一键的迟到press/当前release仍拒绝 |
| T2-L4 迟到批改派至可用执行Tick | 先保留已准入pending与本次准时批的目标；本次迟到批按原始Tick顺序寻找不冲突的最早未封存Tick，且从原始t算起不超maxQueueHop；无合法目标则拒绝 | 保留一原始批一执行Tick，但显式改变固定q=P+1规则；增加占位搜索和边界证明，可能改变timer相对位置、phase资格与等待。原始Tick不变，不是适配层合成捕获时间 |

L4只做一次准入路由，不把已接受批反复转发；若准时批碰到此前已接受的占位，不能搬动旧批，
仍须拒绝新事务。改派后的输入继续满足原始t与dispatchTick的窗口/phase检查，不能因找到空位
就算命中。它要显式修订late-policy，范围显著大于L1。

推荐L1：沿用现有迟到路由，将T2的一个执行Tick一个输入批贯彻到底，避免来源重叠等分支和
新调度算法。来源内使用原始Tick与I1位置重建序列，不以全局到达顺序裁决；所有结果仍只在
相同admission/control轨迹下比较，不承诺任意交付延迟等价。

选择L1并不自动选择执行批封存许可；AD-06仍须决定何时可安全执行该pending集合。
选择L2/L3则必须在支持范围中公开其稳定拒绝形状。四者均不自行补生产阈值。

### 本次落约与验证影响

已修订profile §3.3的dispatch碰撞与单输入限制，新增§3.6，并同步O1/I1/T2-C1引用、
Spec/ABI说明与execution typed错误表；原文与处理范围如下。

| 修订前原文 | 处理 |
| --- | --- |
| “同Tick的第二个不同canonical subject仍按S7A-2 same_tick_collision拒绝” | T2按I1区分发生；合格distinct发生不因共享原始Tick被拒，重复/槽位冲突仍拒绝 |
| “转发导致dispatchTick碰撞时也拒绝第二个distinct subject” | 同未封存q的合格输入按L1合并，不再仅因共享dispatch而拒绝 |
| “按canonical subjects的整体集合验证；批量submit的碰撞使整批失败，不能按数组顺序选一个” | 保留整体校验、其它失败的原子拒绝和禁止静默挑选；共享q本身不再是失败 |
| “现有单条API对已接收状态的拒绝是声明的admission行为；无权把它改为多输入仲裁” | 由owner已选T2/O1/I1/C1/L1的设计合同显式替代；不表示现有API或实现已修改 |
| “一个Tick可有多个timer和至多一个admitted input；timer释放后该输入可使用free slot” | 一个执行Tick处理一个多成员输入批；timer阶段保持，不在输入子批间交错timer |
| typed错误表：dispatch_collision条件为“两个distinct subjects调度到同dispatchTick” | 标为现行单输入实现条件；T2-L1中合格输入共享q不再单独触发此码，不复用旧码表达其它错误 |

逻辑影响：原始Tick/I1身份不变，来源内按原始Tick和位置复算顺序；每个成员保留各自的
admissionFrontier/admissionHorizon/wasForwarded，不能在合并时统一覆盖。旧pending和已seal
结果在新准入失败时保持。来源优先仍可能让较新来源先执行，不引入全局原始时间优先。

Replay/Seek的后续实现必须撤销dispatch唯一性假设，同时保留逐成员身份与准入轨迹；
当前代码`ExecutionKernel`的恢复校验还检查dispatch唯一性，不能只删submit阶段的碰撞检查。
本次不改该代码、历史fixture或Recovery格式，也不预选新revision/码值。

受影响测试（本轮未编写/运行）：kernel旧dispatch碰撞fixture改为有条件合并；同来源跨原始Tick
先后、不同来源O1优先、同次与旧pending合并、重复/槽位冲突原子拒绝、超跨度/finalized/
已seal覆盖拒绝、成员receipt保持，以及独立reference与完整Replay/Seek重建。Fact ordinal、
版本和AD-06推进许可仍未闭合，本裁定不是B0退出证据。

### T2-L1补充核对：确定预期与合同边界

以下均是已选profile §3.6的纸面推导，不是新增裁定或运行结果。所有允许合并的场景均以
通过其它准入检查、目标执行Tick尚未封存为前提；编号和Tick仅用于场景说明，不是生产预算。

| 场景 | 确定预期 |
| --- | --- |
| 同来源原始900的press与原始901的release均映入901，二者I1位置均为0 | 合入一个执行批；来源内先900 press后901 release。原始Tick区分身份，不在合并后重编号 |
| 来源结构键D<F，原始901的D press与原始900的F press均映入901 | 先处理D后处理F；O1来源优先，不按跨来源原始Tick全局排序，不保证两者都能命中 |
| 原始900的成员已在pending，随后原始901的合格成员映入同一q | 保留旧成员逐项准入证据，按同一合并规则加入新成员；不能用新回执覆盖旧成员frontier/horizon |
| 新准入事务含可合并成员，同时含I1重复或同槽位异内容成员 | 原子拒绝本次事务，旧pending/ID/去重记录保持；不折叠非法成员或只发布合法子集 |
| 原始批已关闭，但成员finalized、超跨度或会改写已seal覆盖 | 依原准入规则拒绝；捕获关批和共享q不能豁免失败，不重开sealed Tick或改派空闲Tick |
| 一个q的所有合格成员已合并，但执行/封存许可尚未成立 | 合并完成不等于可以推进或封存；后续AD06-G1已规定原请求等待，完整协议仍须闭合，不补锁、超时或默认许可 |

| 影响面 | 已明确的T2-L1边界 | 尚未由本裁定闭合的内容 |
| --- | --- | --- |
| identity | 原始Tick、I1局部位置和内容身份保持；dispatchTick不是发生唯一键 | I1整数表示、具体typed载体及稳定诊断映射 |
| Replay/Seek/Recovery | 逐成员保留身份和准入轨迹，复算O1/L1输入顺序；不得假设dispatch唯一 | Fact origin/ordinal、semantic revision及Recovery物理编码/兼容迁移 |
| 错误与原子性 | 合格成员共享q不再单独触发dispatch_collision；其余准入失败原子拒绝 | 不删除历史码或复用旧码；新增映射须在首用前另行落约 |
| 状态转换 | 原始批关闭、交付、准入、pending合并、执行封存分别记录；旧sealed结果不变 | AD-06推进许可及Pause/切段控制停点；不新增会话状态或恢复算法 |
| API与版本 | 本轮只改设计文档；现有公共API、SDK版本与Recovery格式保持 | T2所需版本裁定不能由现有token或旧字段布局推定 |

续修同时消除三处基线歧义：profile §3.3明确去重行按I1解释，§7.2标明旧Fact表示的适用范围；
typed补充区分现行碰撞诊断与T2合法发生；架构方案AD-05将旧碰撞条款标为修订前依据。
上述订正不改变O1/I1/C1/L1选择，也不扩大首用消费者准入。

### T2-L1文档续修验证（2026-10-11）

本次按owner“继续完成对T2-L1的文档修订”收尾；续修基于同一`stage-7`/`7ca1fc6`及
34个未提交条目的工作树，未把HEAD当作完整验证身份。以下是本次工作树的文档检查，
不沿用中断前其它裁定轮次的通过结果，不新增C++运行或设备证据。

| 检查 | 结果 |
| --- | --- |
| `python -B tools/check_docs.py` | 通过：421份Markdown、20份candidate JSON/CXT |
| `python -B tools/check_docs_status_contract_tests.py` | 4项通过 |
| `python -B tools/check_docs_section_contract_tests.py` | 5项通过 |
| `python -B tools/check_docs_target_contract_tests.py` | 2项通过 |
| execution profile与生产架构方案的定向片段锚点复算 | 28处链接通过；覆盖状态页、计划、B0记录、架构方案、Spec/profile、ABI/typed及实时宿主草案中的对应引用；不声称覆盖全库片段 |
| `git diff --check` | 通过 |

确定预期表未编写或运行C++测试；未构建、测量、验收设备或修改公共API/Recovery格式。
本次完成的是T2-L1文档修订，不是T2整体、B0退出或产品实施完成。

## 7. AD-06推进许可：G1已裁定并落约

2026-10-11 owner随后要求继续剩余裁定，直到RPA规划达到“可实施”。这恢复了设计与文档工作，
不授权产品实现、原型或测量；遇到需owner选择的语义仍提供至少四个方案并暂停。
先处理AD-06，因为T2-L1的pending合并不决定何时可以执行/封存。owner随后选择G1，
正常运行合同已写入[execution profile §3.7](../../../../formats/gameplay-v2-execution-profile.md#37-rpa推进许可ad06-g1已裁定未实施)。
G2/G3/G4未采用；下文保留裁定前依据与四方案比较。

### 当前缺口与只读依据

生产架构方案AD-06原建议为：D1发布`deliveredFrontier`（“已送达D2的最小未解决序列边界”），
同时要求`F(H) < deliveredFrontier`。序列位置与校准会话Tick不是同一量，不能直接比较；
发送成功、完整接收、准入完成也不是同一事实。该建议尚未构成可实施的许可协议。

现行[execution profile §3.2](../../../../formats/gameplay-v2-execution-profile.md#32-两个时钟游标)
与`ExecutionKernel::advanceRecorded`表明：请求H对应的目标处理前沿为
`P_target=checked(H-C)`，其中C是`windowCloseThreshold`；本节沿宿主草案将此函数记为F(H)。
这里的F(H)不是`finalizationWatermark`参数，也不是Beat→Tick累计函数。空Tick同样推进处理前沿，
不能用“当前没有pending输入”证明可以越过未交付输入。以上为只读核对，未运行产品测试。

### 候选比较的共同约束

- 保持O1/I1/T2-C1/T2-L1；这里选择执行推进协议，不重选捕获方聚合。
- 在正常Running、同一有效时钟段内，候选时间界B表示：所有原始Tick严格小于B的捕获输入
  已有完整性证据，相关发布对象在D2接收完整，且准入或拒绝结局已确定、无未解决缺口。
  原始Tick等于B的批仍可能未关闭。序列前缀只用于核验覆盖，不作为B的数值。
- D1提供捕获关闭与成员覆盖证据，D2结合实际接收和处理结果核验；仅凭D1发送成功不能推进B。
  边界未建立或失效时不使用0、最新收到的sequence或最大已见Tick代替完整性证据。
- 拟执行的目标必须满足`F(H) < B`，等号不够；已准入future pending保留，不为清空队列推进H。
  时钟/控制段失效另行处理，不能将本节正常运行候选当作Pause/Seek/reload停点合同。
- 不能靠超时、队列暂空、伪造捕获Tick或同步阻塞主线程取得许可。已sealed结果不重开。
  等待输入前缀、时钟观察无效、执行积压与恢复静默须可区分；本节不选具体状态枚举或诊断码。
- B只是比较候选的逻辑量，不新增公共字段或Recovery编码；具体载体、极值表示与控制失效规则
  在方向选定后逐条落约。各方案改变交付/advance轨迹时，不预先声称结果完全等价。

### 四种方案

| 方案 | 许可与调度方式 | 维护成本与行为代价 |
| --- | --- | --- |
| AD06-G1 累积完整前缀、原请求等待（已选择） | D2核验并维护B；原H只有在F(H)<B时才调用kernel，否则保留待处理请求并报告原因；不改写H | 一个连续前缀加一个许可谓词，最贴近现有exact-H合同；即使前方有一小段安全区间，较大的请求也会等待，主线程卡顿仍可拖住进度 |
| AD06-G2 显式拆成安全子请求 | 宿主保留目标H_goal，在提交kernel前另生成较小、可表示且单调的H_safe，满足F(H_safe)<B；分别记录目标与实际调用，之后再追赶 | 可先推进已完整前缀；新增目标/子请求调度、重试与记录，不得在kernel内静默夹紧或把H_safe冒充H_goal。额外advance会改变准入轨迹，需独立比较 |
| AD06-G3 按请求异步取得覆盖凭据 | D2为具体H提出非阻塞覆盖请求，D1依T2-C1取得关批证据并关联回复；D2收齐相应输入并确定准入结局后才执行原H | 每次推进的证据关联明确；增加请求ID、重复/过期回复、取消与失效管理，主线程阻塞仍会延迟回复，不能同步等待 |
| AD06-G4 捕获方统一发布输入与推进命令 | D1依T2-C1形成完整批，并把覆盖声明及由有效时钟导出的H按序放入同一通道；D2先处理前缀，再校验并执行原H；其它域不绕过该通道发普通推进 | 正常推进与输入的顺序集中在一个生产者；主线程承担推进发布职责，捕获或present阻塞会同时阻止新推进命令，控制/恢复接入须另行闭合 |

G2是显式修改宿主调度策略的候选，不是允许改变已提交给kernel的exact H；若不存在合法且能前进的
H_safe，仍等待，不饱和、不回绕。G4沿用已选捕获方聚合，仅把推进命令纳入有序通道，
不改成T2-C2的执行方拼片。四方案都不能省略完整性与失效校验。

### 最小对照与推荐依据

只作纸面例子：C=3，已核验B=101，期望H=105，所以F(H)=102。Tick101/102仍缺完整性证据。

| 方案 | 本例的确定候选预期 |
| --- | --- |
| G1 | 原H=105等待；同段B至少达到103且其它条件满足后才执行 |
| G2 | 若单调性与其它条件允许，可明确提交H_safe=103，仅推进到100；H_goal=105仍未完成 |
| G3 | 发起覆盖H=105的异步请求；回复及实际接收/准入闭合前不调用advance(105) |
| G4 | 不发布或不执行缺少覆盖的105推进命令；待其之前的完整前缀处理后才执行，FIFO本身不等于覆盖证据 |

推荐G1，依据是维护性优先：沿用exact-H，只新增经验证的时间前缀与等待条件；避免引入
子请求调度器或每次推进的请求/取消生命周期。代价是接受原请求等待，不把它说成消除主线程卡顿。
选择G1仍需后续明确边界载体、发布/接收/拒绝确认与控制停点，不能只把草案变量改名就宣称AD-06完成。

后续受影响合同：宿主边界的Input delivery progress/Advance request、execution profile的
准入与推进前置、B4控制交付；相应测试应覆盖等号边界、空输入区间、缺口/重复发布、过期段、
原子拒绝、future pending和相同admission/control轨迹下的完整Replay。均未实施或运行。

### G1落约、替换原文与验证影响

| 原文或旧表述 | 本次处理 |
| --- | --- |
| 架构AD-06：“D1发布deliveredFrontier（已送达D2的最小未解决序列边界）” | D1提供捕获证据，D2核验实际接收/准入结局形成同Tick域B；不将sequence与Tick直接比较 |
| 架构AD-06：“候选许可为F(H)<B，不成立就不封存” | 选定正常运行许可；不足时在宿主侧保留原H，不调用advance、不夹紧、不拆分；不使等待阻塞输入交付 |
| 宿主Advance request只写“exact H仍按既有F(H)执行” | 增加G1准入前置与等待说明；已有exact-H、生命周期、checked和失败prefix规则保持 |
| B4进入条件“D-AD06裁定” | 明确须完整合同闭包；G1方向已选不代表积压、证据载体和控制停点已完成 |

identity/Replay：原始输入与I1不变，B不是canonical发生身份；等待不伪造advance记录。
实际调用及结果保留原录制要求；新请求积压策略可能改变control轨迹，仍须独立裁定。
错误/状态：无许可为宿主待处理状态，不增加kernel SessionState、不假造成功回执；
数值/单调性/生命周期错误仍按现行边界处理，不吞成等待。B失效不得继续使用旧证据。
本次新增逻辑前置及文档指针，不定义公共异步方法、稳定码、物理载体或Recovery新格式。

纸面预期：C=3、H=105时目标前沿102；B=101或102均等待，B=103且同段有效才可调用105。
等待自身保持lastAdvanceHorizon/processed frontier/Fact/Fold，正常输入准入可继续；future pending不强行清空。
受影响测试（待授权实施）：严格/等号/负Tick/checked极值、空区间、缺口与重复证明、旧段拒绝、
等待不改变kernel状态且允许交付、失败不假成功、原H保持及同轨迹完整Replay；本轮未编写或运行C++测试。

## 8. 推进请求积压：Q1已裁定并落约

owner于2026-10-11选择Q1，合同归
[execution profile §3.7.1](../../../../formats/gameplay-v2-execution-profile.md#371-推进请求积压ad06-q1已裁定未实施)。
Q2/Q3/Q4未采用。以下保留裁定前问题与比较，随后列出本次合同影响。

G1只决定已保留请求怎样等待，并未定义等待时又收到新H的处理。每次采样都产生请求的宿主，
可能在H=105仍等待时又提出106、107。静默取最新值会丢掉105/106的请求结局；无限排队又无法
给出可实施的容量与失败规则。不同策略还会改变输入准入与advance交织，不能当容器实现细节。

本节仅讨论同一有效段内、尚未开始kernel执行的普通推进请求；四种候选均不将原H拆小，
也不改写已开始/已完成请求。输入交付与控制处理不被等待槽或推进队列阻塞。
显式替代必须使旧请求有可观察的“未执行、被替代”结局，不是假成功；请求关联方式、
控制取消及物理字段另行落约。以下比较中Q1已采纳，其它方案未采纳。

| 方案 | 处理方式 | 维护成本与代价 |
| --- | --- | --- |
| AD06-Q1 单个等待槽，新请求明确拒绝（已选择） | 保留105；106/107在推进调度入口返回忙/未接纳，调用方稍后重新提出目标；已有请求内容不变 | 状态最少，无内部推进队列或隐式覆盖；调用方须处理拒绝并再次发起。拒绝只针对推进，不能拒绝或丢弃输入 |
| AD06-Q2 有限FIFO，每个已接纳请求逐个执行 | 105、106、107依接纳顺序逐项等许可、执行；队列满则明确拒绝新请求 | 请求路径最直观但要维护队列、容量声明和每项回执；恢复交付后可能花时间执行过时中间目标。容量不得由实现补默认值 |
| AD06-Q3 显式以新目标替代尚未执行请求 | 106使105结束为被替代，107再替代106；只对107等待许可并执行，替代与执行开始须串行裁决 | 保留最新推进目标、一个槽；新增替代结局，旧目标可能一直未执行，持续更新可能延后进度。改变实际advance轨迹，不能声称与逐个执行等价 |
| AD06-Q4 固定当前请求，加一个可替代后继槽 | 当前105保持；106进入后继槽，107显式替代106；105处理后再处理107 | 当前请求不会被饿死且能保留最新后继；需要两个槽、后继替代结局及提升顺序，比Q1更复杂 |

Q3/Q4中的替代是需要本次owner显式选择的协议扩展，不是G1已经授权的覆盖。
Q2的队列数量属于设计容量问题，本节不选数值；任何候选都不承诺任意速率、任意长卡顿下无界接纳。
新的时钟观察可按其独立有效性合同更新，不等于已接纳新的推进请求；Q1不要求丢掉时钟证据。

最小对照：105/106/107依次提出、许可暂不足。Q1只接纳105；Q2在容量足够时接纳并保留三项；
Q3最终仅107仍待执行，105/106均有被替代结局；Q4保留105和107，106有被替代结局。
随后输入与证据补齐，四者仍按G1逐请求核验，不因队列非空就强行推进。

推荐Q1：维护性优先，保留每个已接纳原请求，入口明确背压；后续驱动在请求完成后按有效时钟
提出新目标即可。代价是调用方需要显式重试，不提供“每次采样请求都接纳”的保证。
等待不能回退成锁住owner；否则输入无法补齐，会把背压变成死锁。

### Q1替换原文与影响

profile §3.7原第8项“等待期间到达的新H如何排队、拒绝或显式替代……仍须分别落约”中，
普通推进部分由§3.7.1的单等待槽与明确拒绝替代，控制失效仍开放。
宿主Advance request、调度段及Spec/ABI导语同步指向该合同，不复制公共类型。

新普通请求被拒绝不改变旧请求、B或kernel状态，也不增加输入identity/Fact；
请求目标相等不等于同一请求，更不等于执行成功。未接纳请求不自动执行，调用方重新提出时
按当时上下文校验；具体请求关联方式仍须首用前闭合。单槽不限制输入/控制通道容量，
不禁止正常输入准入或有效时钟观察更新，不授予多线程或重入advance许可。

Replay只按实际调用及结果保留既有执行记录；忙/未接纳不是kernel已运行失败。
宿主结果类型与稳定码尚未选择；本次不改公共API、Recovery或SDK版本。
受影响测试（未实施/运行）：105占槽时106/107及新的105均未接纳、旧请求不变；
输入/证据继续到达；许可后仅105执行；未接纳目标不自动补执行；新请求在旧请求结局后重新
校验；Faulted不能因槽空而恢复推进；只在相同实际admission/control轨迹下比较完整Replay。

## 9. 控制处理与未执行推进请求的顺序（K1已选择）

G1/Q1不决定Pause/Seek/Stop/reload与等待推进的先后。例：105已接纳且未开始执行，
随后收到Pause；若先执行105，暂停前可能多封存一段Fact；若取消105，控制从已实际完成的
执行位置继续准备。因此不是普通队列优化，也不能由“控制消息总是覆盖”隐式选择。

### 共同边界

- 仅比较通过基本有效性检查、由owner串行接入处理的显式控制，与尚未开始kernel执行的普通推进。
  消息已发送或进入传输队列不等于控制接入处理，更不等于Pause已经生效。
- 已开始或已完成的advance不靠取消回滚；实际失败/partial prefix、输入准入、future pending、
  sealed Fact与原Replay记录保持。故障处置不得为完成正常推进而强行执行；故障/关闭安全路径
  不受本节的普通等待策略拖延。
- 取消推进只产生可关联的“未执行、已取消”结局，不取消输入、不伪造成功H；控制本身的
  捕获cut、R2状态对齐、音频停点、失败回执仍须分别闭合，不能因此宣告组合Pause已完成。
- owner串行裁决控制接入与advance开始的先后；不得让取消与执行同时各报成功。
  在控制准备期间，输入/证明仍可处理，但不得用旧段证据启动普通推进。

### 四种方案（历史比较；owner随后选择K1）

| 方案 | 未开始推进与控制的顺序 | 代价及失败行为 |
| --- | --- | --- |
| AD06-K1 控制接入即取消未执行推进（推荐） | owner接入有效控制时，先将等待105结束为未执行/已取消，再按现有已完成位置准备控制；控制准备失败也不复活105 | 状态最少，不为旧目标延迟控制；失败后继续Running时，调用方须重新提出推进。控制cut及设备停点仍另行确认 |
| AD06-K2 先完成原推进再开始控制 | 继续收输入和证据，G1成立后执行105，再准备Pause；等待不能先冻结其所需捕获时钟 | 保留已接纳目标的执行机会；控制可能等很久，若证据永久不可得则不能承诺正常完成，不能用超时伪造许可。执行故障走故障处置，不强行完成105 |
| AD06-K3 控制接入时只检查一次许可 | owner接入控制时核验现有证据：若105已获许可且上下文有效，先完整尝试105；否则明确取消105，再准备控制 | 可利用已经到达的证据且不为缺失证据继续等；暂停前是否多推进取决于接入边界，增加分支及对应Replay/失败分析，不能拆小105 |
| AD06-K4 暂存原请求，控制准备成功才取消 | 暂时禁止105开始，先准备控制；若控制可提交则取消105后提交，若控制在保持原组合的阶段拒绝，重新核验后恢复105等待 | 控制准备失败时可保留已接纳请求；新增挂起/恢复状态。旧上下文、时钟段已失效或已发生不可逆效果时不能恢复请求，必须据实际结局处理 |

K4不允许把“软件准备被拒绝”与“设备已部分执行”混同，也不把旧B跨段复用。
K2不适用于强制要求立刻结束故障会话的处置；无论选哪项，都不能阻塞音频callback或主线程等待。
K3先执行时，若kernel实际失败仍按partial prefix报告，不能假装接入前状态未变。

最小对照：若接入Pause时105仍无许可，K1/K3取消105，K2继续等待，K4挂起105并准备Pause。
若此时已有许可但105尚未开始，K1仍取消，K2/K3先尝试105，K4仍先准备控制。
若控制准备被拒绝且保持原组合有效，K1/K3已取消的请求不自动复活；K4可恢复原等待；
K2中原推进已被尝试，不能为控制失败回滚它。以上是纸面候选预期，不是设备证据。

owner已选择K1；采用理由：控制响应不依赖旧H的输入覆盖，且不增加请求挂起/复活状态。代价是有效控制
一经接入，就可能取消一个后来已具备许可的旧目标；控制准备失败后，宿主需要重新请求推进。
K1选择不决定Pause的确切逻辑停点、cut或R2 Fact来源，也不改变已选正常运行G1/Q1。

## 10. K1落约与委托裁定收口

owner已选择K1，并授权剩余事项除极难决策者由代理合理裁定。
K1落于execution profile §3.7.2；其余执行/控制/Fact/时间/兼容合同归
[RPA补充Spec](../../../../formats/rpa-execution-contract.md)，表示/资源/回执归
[宿主边界](../../../../api/realtime-host-boundary.md)。本节替代本记录早先“等待K1–K4”和“其余未裁定”的停点。
前文每轮未实施/未验证的证据声明仍然成立，不能把本轮后续合同倒记成更早已经选定。

| 被替换的原条款/位置 | 原文或最小原文摘录 | RPA替代规则与影响 |
| --- | --- | --- |
| profile §3.7第8项 | “控制命令如何使等待请求失效仍须落约” | K1有效控制接入取消未开始推进；准备失败不复活 |
| profile §3.5正常关批边界 | “时钟冻结/失效、Tick最大值或Pause/切段时可能不可取得” | 正常仍C1；Pause用显式EndRunEpoch与inclusive q，不伪造后续观察；溢出/失效不假报成功 |
| Playback lifecycle旧Pause/Resume | “Pause 禁止 submit/advance，保持已发布画面和真实 held contact；Resume 不补 release。” | 保留普通Paused禁令；新增cut/drain及R2 control commit，真实中断失败，不借普通release |
| Playback旧拒绝控制行 | “不变”“旧组合保持” | 前置拒绝仍不变；RPA已drain/software applied后失败保留实际新前缀，不能宣称完整回滚 |
| profile §7.2 observation origin | “(dispatchTick,CanonicalObservationKey)” | RPA引用完整I1 occurrence，新增rank4 reconciliation、controlOrdinal，FactSemantic与engine identity升级 |
| execution typed supplement旧contact/receipt | “CanonicalObservationKey startPressKey” | 所有发生引用统一升级，含pending、snapshot、Replay，不能只改运行key |
| host草案TB03 | “保留当前执行 profile 拒绝” | T2/O1/I1/C1/L1已选；新profile合格共享Tick不拒绝 |
| host草案TB07 | “这是建议，未获owner语义裁定” | R2已直接选择，细节由委托授权补齐，完整轨迹续跑；缺口拒绝直接Resume |
| host资源准备 | “具体 DTO、token/回执种类……尚需 typed review” | P3-a载体、generation/fence、主线程GPU和worker校验已确定；无公共共享Prepared |
| AudioClockSnapshot现行字段 | source/presentedFrame/sampleRate/estimatedOutputLatencyMs，无采样时刻 | 设计新增独立连贯观察接口，旧字段兼容；不冒充已经改过头文件 |
| Recovery codec基线 | envelope revision2、schema1、canonical.discrete.le.v1 | 设计revision3/schema2/新event codec；旧载体版本拒绝、原件保留 |
| 早期AD02/TB06停点 | 待owner选方向 | 首版同worker/显式tail方向已选；具体参数走具名测量gate，无隐式默认 |

实现影响：B3升级typed carrier/identity/registry/codec/golden/诊断投影和T2/R2内核，
B4闭合公共facade/宿主/资源/设备回执，B5实施线程和设备验收；每批详见计划。
[规划就绪审查](2026-10-11-implementation-readiness.md)登记纸面反例与本轮检查结果。
本轮不修改源码、公共头、schema/codec字节或版本文件；B0退出是设计退出，不是RPA阶段关闭。
