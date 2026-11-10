#include "audio_worker.hpp"
#include "execution_kernel_internal.hpp"
#include "frame_delivery.hpp"
#include "session_worker.hpp"

#include <cstdio>
#include <optional>

using namespace cuexis;
using namespace cuexis::playback;
using namespace cuexis::playback::detail;

namespace {
struct Row final {
    const char* name;
    std::size_t bytes;
};

const Row rows[] = {
    {"SessionCommand", sizeof(SessionCommand)},
    {"SessionReply", sizeof(SessionReply)},
    {"SessionChannelSlot", OwnerChannel<SessionCommand, SessionReply>::slotStorageBytes()},
    {"AudioCommand", sizeof(AudioWorkerCommand)},
    {"AudioReply", sizeof(AudioWorkerReply)},
    {"AudioChannelSlot", OwnerChannel<AudioWorkerCommand, AudioWorkerReply>::slotStorageBytes()},
    {"StepReceipt", sizeof(StepReceipt)},
    {"OwnerStepSlot", OwnerStepLedger::slotStorageBytes()},
    {"ControlReceiptSlot", sizeof(std::optional<ControlReceipt>)},
    {"HistorySlot", FrameDelivery::historySlotStorageBytes()},
    {"ConsumerSlot", FrameDelivery::consumerSlotStorageBytes()},
    {"JournalFrameSlot", FrameObservationLedger::frameSlotStorageBytes()},
    {"FeedbackSlot", sizeof(std::optional<FeedbackObservation>)},
    {"CaptureDelivery", sizeof(GameplayCaptureDelivery)},
    {"Occurrence", sizeof(GameplayOccurrence)},
    {"PauseTransition", sizeof(GameplayPauseTransition)},
    {"ControlReceipt", sizeof(ControlReceipt)},
    {"StepRequest", sizeof(StepRequest)},
    {"HostContext", sizeof(HostContext)},
    {"optional_PresentReceipt", sizeof(std::optional<PresentReceipt>)},
    {"DeliveryFingerprint", sizeof(DeliveryFingerprint)},
    {"DeliveryInfo", sizeof(DeliveryInfo)},
    {"FramePayload", sizeof(FramePayload)},
    {"FrameEnvelope", sizeof(FrameEnvelope)},
    {"FrameAttemptLease", sizeof(FrameAttemptLease)},
    {"DispatchToken", sizeof(DispatchToken)},
    {"FrameObject", sizeof(FrameSnapshot::ObjectSnapshot)},
    {"FeedbackId", sizeof(FeedbackId)},
    {"PortableResourcePtr", sizeof(PortableResourcePtr)},
    {"GameplaySnapshot", sizeof(GameplaySnapshot)},
    {"GameplayReplay", sizeof(GameplayReplay)},
    {"SnapshotDTO", sizeof(judgement::SnapshotDTO)},
    {"ReplayRecord", sizeof(judgement::ReplayRecord)},
    {"KernelFact", sizeof(judgement::KernelFact)},
    {"PhaseProjection", sizeof(judgement::PhaseProjection)},
    {"RequirementRecord", sizeof(judgement::RequirementRecord)},
    {"PreparedRequirement", sizeof(judgement::PreparedRequirement)},
    {"RuntimeState", sizeof(judgement::detail::RuntimeState)},
    {"NativeRecoveryPoint", sizeof(judgement::detail::NativeRecoveryPoint)},
    {"NativeRecoveryIndex", sizeof(judgement::detail::NativeRecoveryIndex)},
    {"NativeRecoveryMaxAnchors", judgement::detail::NativeRecoveryIndex::maxAnchors},
    {"AudioClip", sizeof(audio::AudioClip)},
};
} // namespace

int main() {
    std::printf("{\"compiler\":%d,\"iterator_debug_level\":%d,\"sizes\":{", _MSC_FULL_VER,
                _ITERATOR_DEBUG_LEVEL);
    bool first = true;
    for (const auto& row : rows) {
        std::printf("%s\"%s\":%zu", first ? "" : ",", row.name, row.bytes);
        first = false;
    }
    std::puts("}}");
}
