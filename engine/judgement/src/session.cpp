// Owning selected-profile execution with the historical rejection-only overloads.

#include "execution_kernel.hpp"
#include "execution_testing.hpp"
#include <cuexis/judgement/judgement_session.hpp>

#include <cuexis/core/thread_checker.hpp>
#include <cuexis/judgement/diagnostic.hpp>

#include <cassert>
#include <memory>
#include <string_view>
#include <utility>

namespace cuexis::judgement {
namespace {

//  临时拒绝 token：这不是诊断码表。
//
//  码表、类别集合与严重度集合属第 6 轮（CM-D04 / P2-04 / P2-06），其创建是接受后的独立工作项，
//  目前没有任何一条码被登记。下面的 token 只为让拒绝在本批次内可比较、可区分、可稳定复现而存在，
//  并且刻意带 `provisional` 字样，避免任何消费者把它当成已登记码。它们只出现在 .cpp 中，
//  绝不写进头文件（ABI 禁止把未登记的码写入公共头）。
constexpr std::string_view kLifecycleOrderCode{"judgement.provisional.session.lifecycle_order"};
constexpr std::string_view kUnfrozenSemanticsCode{"judgement.provisional.semantics.unfrozen"};
//  Spec 9.2 的类别集合是冻结的九类，不新增第十类，所以"会话不在该动词要求的生命周期阶段"这条
//  拒绝必须落在既有九类里：它是一次次序关系错误，按 Spec 3.7.3 第 4、5 条与 7.2 的映射走既有
//  `invalid_relation` 原子失败路径。此前这里输出过 `lifecycle`，那个类别名不在九类里，已删除。
constexpr std::string_view kInvalidRelationCategory{"invalid_relation"};
//  Spec 9.2 的九类里没有裸 "capability" 这个类别名，所以"本批次尚未冻结该语义"这一类拒绝
//  输出 `capability_disabled`：能力 id 是已知的，只是本批次没有启用它。令牌名与输出值一致，
//  避免同一模块内出现两种拼写。
constexpr std::string_view kCapabilityDisabledCategory{"capability_disabled"};
constexpr std::string_view kErrorSeverity{"error"};
constexpr std::string_view kLifecycleOrderSummary{
    "the judgement session is not in the lifecycle phase this verb requires"};
constexpr std::string_view kUnfrozenSummary{
    "the judgement semantics this verb requires are not frozen in this batch"};

//  上下文字段路径 token：它们是上下文而不是稳定码，不参与码的等价比较。
constexpr std::string_view kSessionSection{"session"};
constexpr std::string_view kConfigurePath{"configure"};
constexpr std::string_view kPreparePath{"prepare"};
constexpr std::string_view kSubmitPath{"submit"};
constexpr std::string_view kAdvancePath{"advance"};
constexpr std::string_view kQueryPath{"query"};
constexpr std::string_view kSnapshotPath{"snapshot"};
constexpr std::string_view kSeekPath{"seek"};

//  空 token = 该上下文分量对本条诊断不适用（头文件里的 absence 约定），不是任何未冻结角色的默认值。
constexpr std::string_view kAbsent{};

[[nodiscard]] auto rejection(std::string_view code, std::string_view category,
                             std::string_view summary, std::string_view path) -> core::Error {
    const Diagnostic diagnostic{
        .code = code,
        .category = category,
        .severity = kErrorSeverity,
        //  本批次的拒绝都不改变会话状态，因此不会把会话置为 faulted。
        .faulted = false,
        .summary = summary,
        .context =
            DiagnosticContext{
                .fieldPath = DiagnosticFieldPath{.section = kSessionSection, .path = path},
                .requirement = DiagnosticRequirementRef{.kind = kAbsent, .identity = kAbsent},
                .identity = DiagnosticIdentityComponent{.component = kAbsent, .token = kAbsent},
                .budget = nullptr,
                .capabilityId = kAbsent,
                .remediation = kAbsent,
                .rawTime = {},
            },
    };
    return toError(diagnostic);
}

//  生命周期顺序拒绝：动词要求的阶段尚未进入。
[[nodiscard]] auto lifecycleOrderError(std::string_view path) -> core::Error {
    return rejection(kLifecycleOrderCode, kInvalidRelationCategory, kLifecycleOrderSummary, path);
}

//  未冻结语义拒绝：语义属后续批次，本批次既不实现也不假装成功。
[[nodiscard]] auto unfrozenSemanticsError(std::string_view path) -> core::Error {
    return rejection(kUnfrozenSemanticsCode, kCapabilityDisabledCategory, kUnfrozenSummary, path);
}

} // namespace

//  会话的全部可变状态。每个成员都标注 owner thread、读取时点、快照归属和重置行为。
struct JudgementSession::Impl final {
    Impl() noexcept : owner(), configured(false), prepared(false) {}

    //  owner        owner thread：调用 create() 的线程；单线程唯一 owner。
    //               读取时点：每个生命周期动词入口（assertUsable）。
    //               快照归属：不属于任何投影或快照，二者自持有。
    //               重置行为：reset 不转移所有权；任何动词都不转移所有权。
    //  configured   owner thread only。
    //               读取时点：prepare() 入口决定走哪条拒绝路径。
    //               快照归属：不属于任何投影或快照；载体是自持有的值类型，不从会话拷贝状态。
    //               重置行为：reset 成功时复位为 false；本批次没有任何路径能把它置为 true，
    //               因此"拒绝的 configure 不产生半配置会话"是可观察事实而不是承诺。
    //  prepared     owner thread only。
    //               读取时点：prepare() 的原子性断言；submit/advance/query/snapshot/seek/reset 的
    //               阶段判定；hasPreparedState()。
    //               快照归属：不属于任何投影或快照。
    //               重置行为：reset 成功时复位为 false；本批次没有任何路径能把它置为 true，
    //               所以"拒绝的 prepare 不产生半准备会话"同样是可观察事实。
    core::ThreadChecker owner;
    bool configured;
    bool prepared;
    std::optional<SessionConfiguration> configuration;
    std::unique_ptr<detail::ExecutionKernel> kernel;
};

JudgementSession::JudgementSession(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}

JudgementSession::~JudgementSession() noexcept = default;

JudgementSession::JudgementSession(JudgementSession&& other) noexcept = default;

auto JudgementSession::operator=(JudgementSession&& other) noexcept -> JudgementSession& = default;

void JudgementSession::assertUsable() const noexcept {
    assert(impl_ != nullptr && "a moved-from judgement session must not be used");
    impl_->owner.assertCurrent();
}

auto JudgementSession::create() -> core::Result<JudgementSession> {
    //  空会话不持有任何判定状态、不打开设备、不需要 GPU / 窗口 / 音频 / 渲染后端，
    //  因此 create 是本批次唯一有成功路径的动词。
    auto impl = std::make_unique<Impl>();
    return JudgementSession{std::move(impl)};
}

auto JudgementSession::configure() -> core::Result<void> {
    assertUsable();

    //  拒绝的 configure 不发布任何东西：它不进入 configured 阶段，因此不存在半配置会话。
    //  本批次不提供配置内容的表示，接受空配置并返回成功就是"伪成功"。
    return core::unexpected(unfrozenSemanticsError(kConfigurePath));
}

auto JudgementSession::prepare() -> core::Result<void> {
    assertUsable();

    //  原子性不变量（断言层面）：下面两条路径都不写任何状态，因此拒绝的 prepare 不产生半准备会话。
    if (!impl_->configured) {
        //  ABI 生命周期顺序为 create configuration -> prepare；未配置的会话在读取或写入任何
        //  prepared 内容之前就拒绝。

        return core::unexpected(lifecycleOrderError(kPreparePath));
    }

    //  已配置的会话在本批次仍然不可 prepare：prepared 的 requirement 记录、preparedGrace、
    //  仲裁策略与 Interface 投影都没有冻结表示，prepare 只能拒绝，不能发布猜出来的表示。

    return core::unexpected(unfrozenSemanticsError(kPreparePath));
}

auto JudgementSession::submit() -> core::Result<void> {
    assertUsable();

    //  运行期不可变性（断言层面）：进入 prepared 阶段的唯一入口是 prepare，而它稳定拒绝；
    //  因此 requirement 集合、preparedGrace、仲裁策略与 Interface 投影在运行期不可能被修改。
    //  类型层面的形式：它们没有 setter、没有非 const 访问器、也没有任何动词参数。

    if (!impl_->prepared) {
        return core::unexpected(lifecycleOrderError(kSubmitPath));
    }

    //  只有在 prepared 会话存在时才会到达这里；本批次没有能进入该阶段的路径。
    return core::unexpected(unfrozenSemanticsError(kSubmitPath));
}

auto JudgementSession::advance() -> core::Result<void> {
    assertUsable();

    if (!impl_->prepared) {
        return core::unexpected(lifecycleOrderError(kAdvancePath));
    }

    return core::unexpected(unfrozenSemanticsError(kAdvancePath));
}

auto JudgementSession::query() const -> core::Result<JudgementProjection> {
    assertUsable();

    if (impl_->kernel) {
        return JudgementProjection{impl_->kernel->query()};
    }

    //  读取动词是 const 限定的：类型层面不存在经由读取修改会话的路径。

    if (!impl_->prepared) {
        return core::unexpected(lifecycleOrderError(kQueryPath));
    }

    return core::unexpected(unfrozenSemanticsError(kQueryPath));
}

auto JudgementSession::snapshot() const -> core::Result<SnapshotPayload> {
    assertUsable();

    if (!impl_->prepared) {
        return core::unexpected(lifecycleOrderError(kSnapshotPath));
    }

    return core::unexpected(unfrozenSemanticsError(kSnapshotPath));
}

auto JudgementSession::seek() -> core::Result<void> {
    assertUsable();

    if (!impl_->prepared) {
        return core::unexpected(lifecycleOrderError(kSeekPath));
    }

    return core::unexpected(unfrozenSemanticsError(kSeekPath));
}

auto JudgementSession::reset() -> core::Result<void> {
    assertUsable();
    impl_->kernel.reset();
    impl_->configuration.reset();
    impl_->configured = false;
    impl_->prepared = false;
    return {};
}

auto JudgementSession::configure(const SessionConfiguration& configuration)
    -> core::Result<void> try {
    assertUsable();
    if (impl_->configured || impl_->prepared) {
        return core::unexpected(rejection("judgement.s7a4.session.lifecycle_order",
                                          kInvalidRelationCategory, kLifecycleOrderSummary,
                                          kConfigurePath));
    }
    auto valid = detail::validateSessionConfiguration(configuration);
    if (!valid) {
        return core::unexpected(valid.error());
    }
    auto owned = configuration;
    impl_->configuration = std::move(owned);
    impl_->configured = true;
    return {};
} catch (const std::exception&) {
    return core::unexpected(rejection("judgement.s7a4.execution.relation_invalid",
                                      "invalid_relation", "configuration storage allocation failed",
                                      kConfigurePath));
}

auto JudgementSession::prepare(const PreparedGameplay& prepared) -> core::Result<void> {
    assertUsable();
    if (!impl_->configured || impl_->prepared) {
        return core::unexpected(rejection("judgement.s7a4.session.lifecycle_order",
                                          kInvalidRelationCategory, kLifecycleOrderSummary,
                                          kPreparePath));
    }
    auto kernel = detail::ExecutionKernel::prepare(*impl_->configuration, prepared);
    if (!kernel) {
        return core::unexpected(kernel.error());
    }
    impl_->kernel = std::move(*kernel);
    impl_->prepared = true;
    return {};
}

auto JudgementSession::submit(std::vector<ClockedIngress> batch)
    -> core::Result<std::vector<InputReceiptPending>> {
    assertUsable();
    if (!impl_->kernel) {
        return core::unexpected(rejection("judgement.s7a4.session.lifecycle_order",
                                          kInvalidRelationCategory, kLifecycleOrderSummary,
                                          kSubmitPath));
    }
    return impl_->kernel->submit(std::move(batch));
}

auto JudgementSession::advance(Tick horizon) -> core::Result<JudgementProjection> {
    assertUsable();
    if (!impl_->kernel) {
        return core::unexpected(rejection("judgement.s7a4.session.lifecycle_order",
                                          kInvalidRelationCategory, kLifecycleOrderSummary,
                                          kAdvancePath));
    }
    auto result = impl_->kernel->advance(horizon);
    if (!result) {
        return core::unexpected(result.error());
    }
    return JudgementProjection{impl_->kernel->query()};
}

auto JudgementProjection::kernelView() const noexcept -> const KernelProjection& {
    assert(storage_);
    return storage_->projection;
}

auto detail::KernelTestAccess::inject(JudgementSession& session, KernelTestControls controls)
    -> core::Result<void> {
    session.assertUsable();
    if (!session.impl_->kernel) {
        return core::unexpected(lifecycleOrderError(kPreparePath));
    }
    return session.impl_->kernel->inject(std::move(controls));
}
auto detail::KernelTestAccess::visibleSignalCount(const JudgementSession& session, Tick tick)
    -> std::size_t {
    session.assertUsable();
    return session.impl_->kernel ? session.impl_->kernel->visibleSignalCount(tick) : 0;
}

auto JudgementSession::hasPreparedState() const noexcept -> bool {
    assertUsable();
    return impl_->prepared;
}

// Immutable projection storage is shared independently of the session lifetime.
JudgementProjection::JudgementProjection(
    std::shared_ptr<const detail::ProjectionStorage> storage) noexcept
    : storage_(std::move(storage)) {}

SnapshotPayload::SnapshotPayload(std::shared_ptr<const detail::SnapshotStorage> storage) noexcept
    : storage_(std::move(storage)) {}

} // namespace cuexis::judgement
