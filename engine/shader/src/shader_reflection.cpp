#include <cuexis/shader/shader_compiler.hpp>

#include <cuexis/core/error.hpp>
#include <cuexis/shader/shader_diagnostics.hpp>

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace cuexis::shader {
namespace {

constexpr std::string_view reflectionDomain{"cuexis.shader.reflection.v1"};
constexpr std::uint32_t maxUserBinding{16};

[[nodiscard]] auto makeError(std::string message) -> core::Error {
    return core::Error{std::string{diagnosticReflectMismatch}, std::move(message)}.withContext(
        std::string{contextTool}, std::string{toolCuexisShader});
}

void appendU32(std::vector<std::byte>& out, std::uint32_t value) {
    out.push_back(static_cast<std::byte>(value & 0xFFu));
    out.push_back(static_cast<std::byte>((value >> 8) & 0xFFu));
    out.push_back(static_cast<std::byte>((value >> 16) & 0xFFu));
    out.push_back(static_cast<std::byte>((value >> 24) & 0xFFu));
}

void appendBytes(std::vector<std::byte>& out, std::string_view text) {
    const auto* bytes = reinterpret_cast<const std::byte*>(text.data());
    out.insert(out.end(), bytes, bytes + text.size());
}

} // namespace

auto encodeCanonicalReflection(const ShaderReflection& reflection) -> std::vector<std::byte> {
    std::vector<std::byte> encoded;
    appendBytes(encoded, reflectionDomain);
    encoded.push_back(std::byte{0});
    encoded.push_back(reflection.hasCuexisObject ? std::byte{1} : std::byte{0});
    appendU32(encoded, static_cast<std::uint32_t>(reflection.bindings.size()));
    for (const auto& binding : reflection.bindings) {
        appendU32(encoded, binding.set);
        appendU32(encoded, binding.binding);
        appendU32(encoded, static_cast<std::uint32_t>(binding.type));
        appendU32(encoded, static_cast<std::uint32_t>(binding.name.size()));
        appendBytes(encoded, binding.name);
    }
    appendU32(encoded, static_cast<std::uint32_t>(reflection.parameters.size()));
    for (const auto& parameter : reflection.parameters) {
        appendU32(encoded, static_cast<std::uint32_t>(parameter.type));
        appendU32(encoded, parameter.set);
        appendU32(encoded, parameter.binding);
        appendU32(encoded, static_cast<std::uint32_t>(parameter.name.size()));
        appendBytes(encoded, parameter.name);
    }
    return encoded;
}

auto decodeCanonicalReflection(std::span<const std::byte> bytes) -> core::Result<ShaderReflection> {
    std::size_t offset = 0;
    auto remaining = [&]() { return bytes.size() - offset; };
    auto fail = [&](std::string message) {
        return core::unexpected(
            makeError(std::move(message)).withContext("byte_offset", std::to_string(offset)));
    };
    auto readU32 = [&]() -> core::Result<std::uint32_t> {
        if (remaining() < 4) {
            return fail("Canonical reflection is truncated");
        }
        const auto value = static_cast<std::uint32_t>(bytes[offset]) |
                           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
                           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) |
                           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
        offset += 4;
        return value;
    };
    auto readBytes = [&](std::uint32_t count) -> core::Result<std::string> {
        if (remaining() < count) {
            return fail("Canonical reflection string is truncated");
        }
        std::string text(count, '\0');
        std::memcpy(text.data(), bytes.data() + offset, count);
        offset += count;
        return text;
    };

    if (remaining() < reflectionDomain.size() + 2) {
        return fail("Canonical reflection header is truncated");
    }
    const auto domain = std::string_view{reinterpret_cast<const char*>(bytes.data() + offset),
                                         reflectionDomain.size()};
    if (domain != reflectionDomain || bytes[offset + reflectionDomain.size()] != std::byte{0}) {
        return fail("Canonical reflection domain is invalid");
    }
    offset += reflectionDomain.size() + 1;
    const auto flag = bytes[offset];
    ++offset;
    if (flag != std::byte{0} && flag != std::byte{1}) {
        return fail("Canonical reflection CuexisObject flag is invalid");
    }

    ShaderReflection reflection;
    reflection.hasCuexisObject = flag == std::byte{1};
    const auto bindingCount = readU32();
    if (!bindingCount) {
        return core::unexpected(std::move(bindingCount.error()));
    }
    if (*bindingCount > maxUserBinding) {
        return fail("Canonical reflection binding count exceeds the v1 limit");
    }
    reflection.bindings.reserve(*bindingCount);
    for (std::uint32_t index = 0; index < *bindingCount; ++index) {
        ShaderReflectedBinding binding;
        const auto set = readU32();
        const auto bindingIndex = readU32();
        const auto type = readU32();
        const auto nameBytes = readU32();
        if (!set || !bindingIndex || !type || !nameBytes) {
            return fail("Canonical reflection binding record is truncated");
        }
        auto name = readBytes(*nameBytes);
        if (!name) {
            return core::unexpected(std::move(name.error()));
        }
        if (*type < 1 || *type > 7) {
            return fail("Canonical reflection binding type is unsupported");
        }
        binding.set = *set;
        binding.binding = *bindingIndex;
        binding.type = static_cast<ShaderParameterType>(*type);
        binding.name = std::move(*name);
        reflection.bindings.push_back(std::move(binding));
    }

    const auto parameterCount = readU32();
    if (!parameterCount) {
        return core::unexpected(std::move(parameterCount.error()));
    }
    if (*parameterCount > 32) {
        return fail("Canonical reflection parameter count exceeds the v1 limit");
    }
    reflection.parameters.reserve(*parameterCount);
    for (std::uint32_t index = 0; index < *parameterCount; ++index) {
        ShaderReflectedParameter parameter;
        const auto type = readU32();
        const auto set = readU32();
        const auto bindingIndex = readU32();
        const auto nameBytes = readU32();
        if (!type || !set || !bindingIndex || !nameBytes) {
            return fail("Canonical reflection parameter record is truncated");
        }
        auto name = readBytes(*nameBytes);
        if (!name) {
            return core::unexpected(std::move(name.error()));
        }
        if (*type < 1 || *type > 7) {
            return fail("Canonical reflection parameter type is unsupported");
        }
        parameter.type = static_cast<ShaderParameterType>(*type);
        parameter.set = *set;
        parameter.binding = *bindingIndex;
        parameter.name = std::move(*name);
        reflection.parameters.push_back(std::move(parameter));
    }
    if (remaining() != 0) {
        return fail("Canonical reflection has trailing bytes");
    }
    return reflection;
}

} // namespace cuexis::shader
