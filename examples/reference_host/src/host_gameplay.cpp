#include "host_gameplay.hpp"
#include "host_commands.hpp"
#include <array>
#include <charconv>
#include <fstream>
#include <sstream>
#if defined(CUEXIS_PLAYBACK_GAMEPLAY_CANDIDATE)
namespace cuexis_reference_host {
namespace {
struct Failure {
    cuexis::core::Error error;
};
void require(bool value, std::string_view message) {
    if (!value)
        throw Failure{cuexis::core::Error{std::string{diagnostic::syntax}, std::string{message}}};
}
template <class T> auto integer(std::string_view token) -> T {
    T number{};
    const auto parsed = std::from_chars(token.data(), token.data() + token.size(), number);
    require(parsed.ec == std::errc{} && parsed.ptr == token.data() + token.size(),
            "Gameplay input requires exact integer");
    return number;
}
auto read(const std::filesystem::path& path) -> std::string {
    std::ifstream input{path, std::ios::binary | std::ios::ate};
    const auto end = input.tellg();
    require(input && end >= 0 && static_cast<std::uintmax_t>(end) <= maxCommandFileBytes,
            "Gameplay host file unavailable or exceeds command byte bound");
    std::string text(static_cast<std::size_t>(end), '\0');
    input.seekg(0);
    input.read(text.data(), static_cast<std::streamsize>(text.size()));
    require(input.good(), "Gameplay host file read failed");
    return text;
}
} // namespace
auto readGameplayHost(const std::filesystem::path& configuration, std::string_view budgetText,
                      std::string_view horizonText, std::string_view presentationText,
                      const std::optional<std::filesystem::path>& observations)
    -> cuexis::core::Result<GameplayHost> try {
    std::array<std::size_t, 5> values{};
    for (std::size_t i = 0; i < values.size(); ++i) {
        const auto comma = budgetText.find(',');
        require((i + 1 == values.size()) == (comma == std::string_view::npos),
                "Gameplay config budget requires five components");
        values[i] = integer<std::size_t>(budgetText.substr(0, comma));
        require(values[i] > 0, "Gameplay config budget must be positive");
        if (comma != std::string_view::npos)
            budgetText.remove_prefix(comma + 1);
    }
    cuexis::playback::GameplayConfigurationDecodeBudget budget{values[0], values[1], values[2],
                                                               values[3], values[4], true};
    auto decoded = cuexis::playback::decodeGameplayConfiguration(read(configuration), budget);
    if (!decoded)
        return cuexis::core::unexpected(std::move(decoded.error()));
    GameplayHost result{std::move(*decoded),
                        budget,
                        integer<std::int64_t>(horizonText),
                        integer<std::int64_t>(presentationText),
                        {}};
    require(result.horizonStep > 0 && result.presentationStep > 0,
            "Gameplay Tick step must be positive");
    if (observations) {
        std::istringstream lines{read(*observations)};
        std::string line;
        std::optional<std::uint64_t> previous;
        while (std::getline(lines, line)) {
            if (line.empty() || line[0] == '#')
                continue;
            require(result.observations.size() < maxCommands,
                    "Gameplay observation record bound exceeded");
            std::istringstream fields{line};
            std::array<std::string, 13> tokens;
            for (auto& token : tokens)
                require(static_cast<bool>(fields >> token),
                        "Gameplay observation needs thirteen columns");
            std::string trailing;
            require(!(fields >> trailing), "Gameplay observation has extra columns");
            cuexis::playback::GameplayInput input{};
            input.observationTick = {integer<std::int64_t>(tokens[0])};
            input.sequence = integer<std::uint64_t>(tokens[1]);
            require(!previous || input.sequence > *previous,
                    "Gameplay observation sequence must increase");
            previous = input.sequence;
            require(tokens[2] == "press" || tokens[2] == "release",
                    "Gameplay observation supports press/release");
            input.action = tokens[2] == "press" ? cuexis::playback::GameplayInputAction::Press
                                                : cuexis::playback::GameplayInputAction::Release;
            input.channel = tokens[3];
            input.domain = tokens[4];
            input.sourceClass = tokens[5];
            input.rawTimestamps = {
                integer<std::int64_t>(tokens[6]), integer<std::int64_t>(tokens[7]),
                integer<std::int64_t>(tokens[8]), integer<std::int64_t>(tokens[9])};
            for (unsigned i = 10; i < 13; ++i)
                require(tokens[i] == "0" || tokens[i] == "1",
                        "Gameplay provenance flag must be 0/1");
            input.crossedSamplingGap = tokens[10] == "1";
            input.reconnected = tokens[11] == "1";
            input.droppedSamples = tokens[12] == "1";
            result.observations.push_back(std::move(input));
        }
    }
    return result;
} catch (const Failure& failure) {
    return cuexis::core::unexpected(failure.error);
} catch (...) {
    return cuexis::core::unexpected(
        cuexis::core::Error{std::string{diagnostic::fileRead}, "Gameplay host parsing failed"});
}
} // namespace cuexis_reference_host
#endif
