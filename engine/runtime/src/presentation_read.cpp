#include <cuexis/render/camera_component.hpp>
#include <cuexis/render/renderable_component.hpp>
#include <cuexis/runtime/presentation_read.hpp>
#include <cuexis/runtime/runtime_session.hpp>
#include <limits>
#include <thread>

namespace cuexis::runtime::detail {
struct PresentationWindow final {
    const std::thread::id owner{std::this_thread::get_id()};
    std::uint64_t serial{};
    bool active{};
    const entt::registry* registry{};
    const ObjectEntityMap* objects{};
    std::optional<entt::entity> camera;
    PresentationState state;
};

auto PresentationRead::available() const noexcept -> bool {
    return window_ != nullptr && window_->owner == std::this_thread::get_id() && window_->active &&
           window_->serial == serial_;
}
auto PresentationRead::availableError() const -> core::Error {
    if (window_ == nullptr || window_->owner != std::this_thread::get_id())
        return core::Error{"runtime.presentation.view_unavailable",
                           "Presentation read belongs to another owner"};
    return core::Error{"runtime.presentation.view_unavailable",
                       "Presentation read window has ended"};
}
auto PresentationRead::state() const -> core::Result<PresentationState> {
    if (!available())
        return core::unexpected(availableError());
    return window_->state;
}
auto PresentationRead::readObject(std::size_t index) const -> core::Result<ObjectPresentation> {
    if (!available())
        return core::unexpected(availableError());
    const auto& entries = window_->objects->entries();
    if (index >= entries.size())
        return core::unexpected(core::Error{"runtime.presentation.layout_stale",
                                            "Presentation object index is outside the layout"});
    const auto& entry = entries[index];
    const auto& registry = *window_->registry;
    ObjectPresentation object;
    object.id = entry.objectId.value;
    if (!registry.valid(entry.entity))
        return object;
    if (const auto* matrix = registry.try_get<world::WorldTransformComponent>(entry.entity)) {
        object.hasTransform = true;
        object.matrix = matrix->matrix;
    }
    object.hasRenderable = registry.all_of<render::RenderableComponent>(entry.entity);
    if (const auto* appearance = registry.try_get<render::AppearanceComponent>(entry.entity)) {
        object.hasAppearance = true;
        object.visible = appearance->visible;
        object.material = appearance->materialAssetId;
        object.opacity = appearance->opacity;
        object.tint = appearance->tint;
    }
    return object;
}
auto PresentationRead::readSnapshotObject(std::size_t index, ObjectVisualRead& row) const
    -> core::Result<void> {
    row = ObjectVisualRead{};
    if (!available())
        return core::unexpected(availableError());
    const auto& entries = window_->objects->entries();
    if (index >= entries.size())
        return core::unexpected(core::Error{"runtime.presentation.layout_stale",
                                            "Presentation object index is outside the layout"});
    // Gathering only borrows const registry state. Borrowed fields stay within the read callback.
    const auto& entry = entries[index];
    const auto& registry = *window_->registry;
    row.id = entry.objectId.value;
    if (!registry.valid(entry.entity))
        return {};
    if (const auto* transform = registry.try_get<world::WorldTransformComponent>(entry.entity))
        row.matrix = &transform->matrix;
    if (const auto* appearance = registry.try_get<render::AppearanceComponent>(entry.entity)) {
        row.hasAppearance = true;
        row.visible = appearance->visible;
        row.material = appearance->materialAssetId;
        row.opacity = appearance->opacity;
        row.tint = appearance->tint;
    }
    return {};
}
auto PresentationRead::readSnapshotObject(std::size_t index) const
    -> core::Result<ObjectVisualRead> {
    ObjectVisualRead row;
    auto result = readSnapshotObject(index, row);
    if (!result)
        return core::unexpected(std::move(result.error()));
    return row;
}
auto PresentationRead::readCamera() const -> core::Result<CameraPresentation> {
    if (!available())
        return core::unexpected(availableError());
    CameraPresentation value;
    const auto selected = window_->camera;
    value.selected = selected.has_value();
    if (!selected)
        return value;
    const auto& registry = *window_->registry;
    if (!registry.valid(*selected))
        return value;
    const auto* camera = registry.try_get<render::CameraComponent>(*selected);
    if (!camera)
        return value;
    value.present = true;
    value.fovY = camera->fovY;
    value.nearPlane = camera->nearPlane;
    value.farPlane = camera->farPlane;
    value.pitch = camera->pitch;
    value.yaw = camera->yaw;
    value.roll = camera->roll;
    if (const auto* matrix = registry.try_get<world::WorldTransformComponent>(*selected)) {
        value.hasTransform = true;
        value.matrix = matrix->matrix;
    }
    return value;
}
auto PresentationAccess::state(const RuntimeSession& session) -> core::Result<PresentationState> {
    if (!session.threadChecker_.isCurrent() || session.callbackActive_)
        return core::unexpected(core::Error{"runtime.presentation.view_unavailable",
                                            "Presentation state is outside its owner operation"});
    if (!session.world_)
        return core::unexpected(core::Error{"runtime.session.empty", "Runtime has no World"});
    return installedState(session);
}
auto PresentationAccess::installedState(const RuntimeSession& session) noexcept
    -> PresentationState {
    session.threadChecker_.assertCurrent();
    return PresentationState{{session.sessionToken_, session.presentationClock_.layout,
                              session.presentationClock_.version},
                             session.presentationClock_.updates,
                             session.presentationClock_.epoch,
                             session.presentationSource_,
                             session.presentationFrame_};
}
auto PresentationAccess::read(const RuntimeSession& session, PresentationStamp expected,
                              void* context,
                              core::Result<void> (*callback)(void*, const PresentationRead&))
    -> core::Result<void> try {
    auto current = state(session);
    if (!current)
        return core::unexpected(std::move(current.error()));
    if (current->stamp != expected)
        return core::unexpected(core::Error{"runtime.presentation.version_unavailable",
                                            "Presentation state or layout has changed"});
    if (!session.presentationWindow_)
        session.presentationWindow_ = std::make_shared<PresentationWindow>();
    auto& window = *session.presentationWindow_;
    if (window.serial == std::numeric_limits<std::uint64_t>::max())
        return core::unexpected(core::Error{"runtime.session.update_exception",
                                            "Presentation read window capacity exhausted"});
    RuntimeSession::CallbackScope operation{session.callbackActive_};
    return std::as_const(*session.world_)
        .withRegistry([&](const entt::registry& registry) -> core::Result<void> {
            ++window.serial;
            window.registry = &registry;
            window.objects = &session.objects_;
            window.camera = session.presentationCamera_;
            window.state = *current;
            window.active = true;
            struct End final {
                PresentationWindow& window;
                ~End() noexcept {
                    window.active = false;
                    window.registry = nullptr;
                    window.objects = nullptr;
                }
            } end{window};
            const PresentationRead view{session.presentationWindow_, window.serial};
            return callback(context, view);
        });
} catch (const std::bad_alloc&) {
    // Preserve the caller allocation error boundary.
    throw;
} catch (...) {
    return core::unexpected(
        core::Error{"runtime.session.update_exception", "Presentation read preparation failed"});
}
} // namespace cuexis::runtime::detail
