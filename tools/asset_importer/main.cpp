#include <cuexis/shader/shader_cache.hpp>
#include <cuexis/shader/shader_compiler.hpp>
#include <cuexis/shader/shader_diagnostics.hpp>

#include <array>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

void printUsage() {
    std::cerr << "Usage: cuexis_asset_importer --help\n"
              << "       cuexis_asset_importer --compile --vertex <file> --fragment <file>\n"
              << "           [--keyword NAME]... [--declare-keyword NAME]...\n"
              << "           [--binding NAME:TYPE:SET:BINDING]...\n"
              << "           [--cache-dir DIR] [--identity HEX64]\n"
              << "Compiles ShaderAsset GLSL 450 sources through cuexis.importer.shader.v1.\n"
              << "With --cache-dir, writes CXSCCH01 (SPIR-V, GLSL 330, GLSL ES 300, reflection).\n";
}

[[nodiscard]] auto readTextFile(const std::filesystem::path& path) -> std::optional<std::string> {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        return std::nullopt;
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    if (!input && !input.eof()) {
        return std::nullopt;
    }
    return contents.str();
}

[[nodiscard]] auto parseParameterType(std::string_view name)
    -> std::optional<cuexis::shader::ShaderParameterType> {
    using cuexis::shader::ShaderParameterType;
    if (name == "Float") {
        return ShaderParameterType::Float;
    }
    if (name == "Vec2") {
        return ShaderParameterType::Vec2;
    }
    if (name == "Vec3") {
        return ShaderParameterType::Vec3;
    }
    if (name == "Vec4") {
        return ShaderParameterType::Vec4;
    }
    if (name == "Int") {
        return ShaderParameterType::Int;
    }
    if (name == "Bool") {
        return ShaderParameterType::Bool;
    }
    if (name == "Texture2D") {
        return ShaderParameterType::Texture2D;
    }
    return std::nullopt;
}

[[nodiscard]] auto parameterTypeName(cuexis::shader::ShaderParameterType type) -> std::string_view {
    using cuexis::shader::ShaderParameterType;
    switch (type) {
    case ShaderParameterType::Float:
        return "Float";
    case ShaderParameterType::Vec2:
        return "Vec2";
    case ShaderParameterType::Vec3:
        return "Vec3";
    case ShaderParameterType::Vec4:
        return "Vec4";
    case ShaderParameterType::Int:
        return "Int";
    case ShaderParameterType::Bool:
        return "Bool";
    case ShaderParameterType::Texture2D:
        return "Texture2D";
    }
    return "Unknown";
}

struct CompileArgs final {
    std::filesystem::path vertex;
    std::filesystem::path fragment;
    std::vector<std::string> declaredKeywords;
    std::vector<std::string> selectedKeywords;
    std::vector<cuexis::shader::ShaderDeclaredBinding> bindings;
    std::vector<std::string> bindingNames;
    std::filesystem::path cacheDir;
    std::string identityHex;
};

[[nodiscard]] auto parseBinding(std::string_view spec, std::string& ownedName)
    -> std::optional<cuexis::shader::ShaderDeclaredBinding> {
    const auto first = spec.find(':');
    const auto second =
        first == std::string_view::npos ? std::string_view::npos : spec.find(':', first + 1);
    const auto third =
        second == std::string_view::npos ? std::string_view::npos : spec.find(':', second + 1);
    if (first == std::string_view::npos || second == std::string_view::npos ||
        third == std::string_view::npos || spec.find(':', third + 1) != std::string_view::npos) {
        return std::nullopt;
    }

    const auto name = spec.substr(0, first);
    const auto typeName = spec.substr(first + 1, second - first - 1);
    const auto setText = spec.substr(second + 1, third - second - 1);
    const auto bindingText = spec.substr(third + 1);
    const auto type = parseParameterType(typeName);
    if (name.empty() || !type) {
        return std::nullopt;
    }

    try {
        const auto set = static_cast<std::uint32_t>(std::stoul(std::string{setText}));
        const auto binding = static_cast<std::uint32_t>(std::stoul(std::string{bindingText}));
        ownedName = std::string{name};
        return cuexis::shader::ShaderDeclaredBinding{
            .set = set,
            .binding = binding,
            .type = *type,
            .name = ownedName,
        };
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

[[nodiscard]] auto parseCompileArgs(int argc, char** argv) -> std::optional<CompileArgs> {
    if (argc < 2 || std::string_view{argv[1]} != "--compile") {
        return std::nullopt;
    }

    CompileArgs args{};
    for (int index = 2; index < argc; ++index) {
        const std::string_view flag{argv[index]};
        const auto takeValue = [&]() -> std::optional<std::string_view> {
            if (index + 1 >= argc) {
                return std::nullopt;
            }
            ++index;
            return std::string_view{argv[index]};
        };

        if (flag == "--vertex") {
            const auto value = takeValue();
            if (!value || value->empty()) {
                return std::nullopt;
            }
            args.vertex = std::filesystem::path{std::string{*value}};
        } else if (flag == "--fragment") {
            const auto value = takeValue();
            if (!value || value->empty()) {
                return std::nullopt;
            }
            args.fragment = std::filesystem::path{std::string{*value}};
        } else if (flag == "--keyword") {
            const auto value = takeValue();
            if (!value || value->empty()) {
                return std::nullopt;
            }
            args.selectedKeywords.emplace_back(*value);
        } else if (flag == "--declare-keyword") {
            const auto value = takeValue();
            if (!value || value->empty()) {
                return std::nullopt;
            }
            args.declaredKeywords.emplace_back(*value);
        } else if (flag == "--binding") {
            const auto value = takeValue();
            if (!value) {
                return std::nullopt;
            }
            args.bindingNames.emplace_back();
            auto binding = parseBinding(*value, args.bindingNames.back());
            if (!binding) {
                return std::nullopt;
            }
            args.bindings.push_back(*binding);
        } else if (flag == "--cache-dir") {
            const auto value = takeValue();
            if (!value || value->empty()) {
                return std::nullopt;
            }
            args.cacheDir = std::filesystem::path{std::string{*value}};
        } else if (flag == "--identity") {
            const auto value = takeValue();
            if (!value || value->empty()) {
                return std::nullopt;
            }
            args.identityHex = std::string{*value};
        } else {
            return std::nullopt;
        }
    }

    if (args.vertex.empty() || args.fragment.empty()) {
        return std::nullopt;
    }
    return args;
}

[[nodiscard]] auto parseIdentityHex(std::string_view hex)
    -> std::optional<std::array<std::uint8_t, 32>> {
    if (hex.size() != 64) {
        return std::nullopt;
    }
    auto nibble = [](char value) -> int {
        if (value >= '0' && value <= '9') {
            return value - '0';
        }
        if (value >= 'a' && value <= 'f') {
            return value - 'a' + 10;
        }
        if (value >= 'A' && value <= 'F') {
            return value - 'A' + 10;
        }
        return -1;
    };
    std::array<std::uint8_t, 32> identity{};
    for (std::size_t index = 0; index < identity.size(); ++index) {
        const int high = nibble(hex[index * 2U]);
        const int low = nibble(hex[index * 2U + 1U]);
        if (high < 0 || low < 0) {
            return std::nullopt;
        }
        identity[index] = static_cast<std::uint8_t>((high << 4) | low);
    }
    return identity;
}

void printError(const cuexis::core::Error& error) {
    std::cerr << "error.code=" << error.code() << '\n';
    std::cerr << "error.message=" << error.message() << '\n';
    for (const auto& context : error.context()) {
        std::cerr << "error.context." << context.key << '=' << context.value << '\n';
    }
}

int compileSources(const CompileArgs& args) {
    const auto vertex = readTextFile(args.vertex);
    const auto fragment = readTextFile(args.fragment);
    if (!vertex || !fragment) {
        std::cerr << "error.code=" << cuexis::shader::diagnosticCompileFailed << '\n';
        std::cerr << "error.message=Failed to read GLSL source files\n";
        return 1;
    }

    std::vector<std::string_view> declared;
    declared.reserve(args.declaredKeywords.size());
    for (const auto& keyword : args.declaredKeywords) {
        declared.emplace_back(keyword);
    }
    std::vector<std::string_view> selected;
    selected.reserve(args.selectedKeywords.size());
    for (const auto& keyword : args.selectedKeywords) {
        selected.emplace_back(keyword);
    }
    std::vector<cuexis::shader::ShaderDeclaredBinding> bindings = args.bindings;
    std::vector<cuexis::shader::ShaderDeclaredParameter> parameters;
    parameters.reserve(bindings.size());
    for (std::size_t index = 0; index < bindings.size(); ++index) {
        bindings[index].name = args.bindingNames[index];
        parameters.push_back(cuexis::shader::ShaderDeclaredParameter{
            .name = bindings[index].name,
            .type = bindings[index].type,
            .set = bindings[index].set,
            .binding = bindings[index].binding,
        });
    }

    const cuexis::shader::ShaderCompileRequest request{
        .vertexSource = *vertex,
        .fragmentSource = *fragment,
        .declaredKeywords = declared,
        .selectedKeywords = selected,
        .declaredBindings = bindings,
        .declaredParameters = parameters,
    };
    const auto compiled = cuexis::shader::ShaderCompiler::compile(request);
    if (!compiled) {
        printError(compiled.error());
        return 1;
    }

    const auto encoded = cuexis::shader::encodeCanonicalReflection(compiled->reflection);
    std::cout << "status=ok\n";
    std::cout << "vertex_spirv_bytes=" << compiled->vertexSpirv.size() << '\n';
    std::cout << "fragment_spirv_bytes=" << compiled->fragmentSpirv.size() << '\n';
    std::cout << "vertex_glsl330_bytes=" << compiled->vertexGlsl330.size() << '\n';
    std::cout << "fragment_glsl330_bytes=" << compiled->fragmentGlsl330.size() << '\n';
    std::cout << "vertex_glsles300_bytes=" << compiled->vertexGlslEs300.size() << '\n';
    std::cout << "fragment_glsles300_bytes=" << compiled->fragmentGlslEs300.size() << '\n';
    std::cout << "has_cuexis_object=" << (compiled->reflection.hasCuexisObject ? 1 : 0) << '\n';
    std::cout << "binding_count=" << compiled->reflection.bindings.size() << '\n';
    for (const auto& binding : compiled->reflection.bindings) {
        std::cout << "binding=" << binding.name << ' ' << parameterTypeName(binding.type) << ' '
                  << binding.set << ' ' << binding.binding << '\n';
    }
    std::cout << "parameter_count=" << compiled->reflection.parameters.size() << '\n';
    std::cout << "reflection_bytes=" << encoded.size() << '\n';

    if (args.cacheDir.empty()) {
        return 0;
    }

    std::array<std::uint8_t, 32> identity{};
    if (!args.identityHex.empty()) {
        auto parsed = parseIdentityHex(args.identityHex);
        if (!parsed) {
            std::cerr << "error.code=" << cuexis::shader::diagnosticCacheKeyInvalid << '\n';
            std::cerr << "error.message=--identity must be 64 lowercase or uppercase hex digits\n";
            return 1;
        }
        identity = *parsed;
    } else {
        identity = cuexis::shader::hashStandaloneSourceIdentity(*vertex, *fragment, "main", "main",
                                                                selected);
    }

    cuexis::shader::ShaderCacheRecord record;
    record.sourceIdentity = identity;
    record.selectedKeywords = args.selectedKeywords;
    record.artifact = *compiled;
    cuexis::shader::ShaderCacheStore store{args.cacheDir};
    const auto path = store.store(record);
    if (!path) {
        printError(path.error());
        return 1;
    }
    const auto encodedCache = cuexis::shader::encodeCache(record);
    std::cout << "cache_written=1\n";
    std::cout << "cache_path=" << path->generic_string() << '\n';
    if (encodedCache) {
        std::cout << "cache_bytes=" << encodedCache->size() << '\n';
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view{argv[1]} == "--help") {
        printUsage();
        return 0;
    }
    if (argc >= 2 && std::string_view{argv[1]} == "--compile") {
        const auto args = parseCompileArgs(argc, argv);
        if (!args) {
            printUsage();
            return 2;
        }
        return compileSources(*args);
    }
    printUsage();
    return 2;
}
