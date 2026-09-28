#include "host_commands.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>

namespace cuexis_reference_host {

auto verbName(Verb verb) -> std::string_view {
    switch (verb) {
    case Verb::Open:
        return "open";
    case Verb::Play:
        return "play";
    case Verb::Pause:
        return "pause";
    case Verb::Tick:
        return "tick";
    case Verb::Seek:
        return "seek";
    case Verb::Reload:
        return "reload";
    case Verb::Quit:
        return "quit";
    }
    // Unreachable: the switch covers every enumerator. The empty view keeps the
    // fallback visibly wrong instead of inventing a plausible verb name.
    return {};
}

namespace {

// The UTF-8 byte order mark. It is stripped only when it opens the file; the
// same three bytes anywhere else are ordinary data.
constexpr std::string_view utf8Bom = "\xEF\xBB\xBF";

// The whitespace the grammar strips and tokenizes on. A lone carriage return is
// whitespace, not a line ending: only LF, or CRLF, ends a line.
[[nodiscard]] constexpr auto isBlank(char value) noexcept -> bool {
    return value == ' ' || value == '\t' || value == '\v' || value == '\f' || value == '\r';
}

// Strips the leading and trailing whitespace of one physical line.
[[nodiscard]] constexpr auto trimBlank(std::string_view text) noexcept -> std::string_view {
    std::size_t first = 0;
    while (first < text.size() && isBlank(text[first])) {
        ++first;
    }
    std::size_t last = text.size();
    while (last > first && isBlank(text[last - 1])) {
        --last;
    }
    return text.substr(first, last - first);
}

// Advances past whitespace without consuming anything else.
[[nodiscard]] constexpr auto skipBlank(std::string_view text, std::size_t from) noexcept
    -> std::size_t {
    std::size_t cursor = from;
    while (cursor < text.size() && isBlank(text[cursor])) {
        ++cursor;
    }
    return cursor;
}

// Reads the next whitespace-delimited token and leaves cursor just past it. An
// empty view means there was no further token.
[[nodiscard]] constexpr auto takeToken(std::string_view text, std::size_t& cursor) noexcept
    -> std::string_view {
    cursor = skipBlank(text, cursor);
    const std::size_t begin = cursor;
    while (cursor < text.size() && !isBlank(text[cursor])) {
        ++cursor;
    }
    return text.substr(begin, cursor - begin);
}

// Narrow rendering of a path for a diagnostic. Printable ASCII is copied
// through and everything else becomes '?', so building a diagnostic can never
// fail on a path the narrow encoding cannot represent.
[[nodiscard]] auto displayPath(const std::filesystem::path& path) -> std::string {
    using Unit = std::filesystem::path::value_type;
    std::string text;
    for (const Unit unit : path.native()) {
        const bool printable = unit >= static_cast<Unit>(' ') && unit < static_cast<Unit>(0x7F);
        text.push_back(printable ? static_cast<char>(unit) : '?');
    }
    return text;
}

// A rejection that points at one physical source line.
[[nodiscard]] auto failureAtLine(std::string_view code, std::size_t lineNumber,
                                 std::string_view reason) -> ParseFailure {
    return ParseFailure{code, "line " + std::to_string(lineNumber) + ": " + std::string{reason}};
}

// A rejection that is a property of the whole file, so no line applies.
[[nodiscard]] auto failure(std::string_view code, std::string_view reason) -> ParseFailure {
    return ParseFailure{code, std::string{reason}};
}

// The physical line a byte offset sits on, counted the way an editor counts, so
// the number in a diagnostic is the number a reader can jump to.
[[nodiscard]] constexpr auto lineAt(std::string_view content, std::size_t offset) noexcept
    -> std::size_t {
    const std::size_t limit = offset < content.size() ? offset : content.size();
    std::size_t lineNumber = 1;
    for (std::size_t index = 0; index < limit; ++index) {
        if (content[index] == '\n') {
            ++lineNumber;
        }
    }
    return lineNumber;
}

enum class NumberStatus { Ok, NotDecimal, OutOfRange };

// Decimal, non-negative and complete. std::from_chars is asked for a uint64 and
// the answer is accepted only when it consumed the whole token and stayed in
// range, which rejects a sign, a decimal point, an exponent, a hex prefix and
// any trailing character.
[[nodiscard]] auto parseDecimal(std::string_view text, std::uint64_t& value) noexcept
    -> NumberStatus {
    if (text.empty()) {
        return NumberStatus::NotDecimal;
    }
    const char* const first = text.data();
    const char* const last = first + text.size();
    value = 0;
    const std::from_chars_result result = std::from_chars(first, last, value, 10);
    if (result.ec == std::errc::result_out_of_range) {
        return NumberStatus::OutOfRange;
    }
    if (result.ec != std::errc{} || result.ptr != last) {
        return NumberStatus::NotDecimal;
    }
    return NumberStatus::Ok;
}

enum class QuoteStatus { Ok, Unterminated, BadEscape, TrailingText };

// Decodes one double-quoted path argument. text starts at the opening quote and
// runs to the end of the line; the only escapes the project uses are \" and \\.
// The opening quote itself was already checked by the caller, because
// std::quoted would silently accept an unquoted token as a fallback.
[[nodiscard]] auto parseQuotedPath(std::string_view text, std::string& value) -> QuoteStatus {
    value.clear();
    std::size_t cursor = 1;
    bool closed = false;
    while (cursor < text.size()) {
        const char current = text[cursor];
        if (current == '\\') {
            if (cursor + 1U >= text.size()) {
                return QuoteStatus::Unterminated;
            }
            const char escaped = text[cursor + 1U];
            if (escaped != '"' && escaped != '\\') {
                return QuoteStatus::BadEscape;
            }
            value.push_back(escaped);
            cursor += 2U;
            continue;
        }
        if (current == '"') {
            closed = true;
            ++cursor;
            break;
        }
        value.push_back(current);
        ++cursor;
    }
    if (!closed) {
        return QuoteStatus::Unterminated;
    }
    if (skipBlank(text, cursor) != text.size()) {
        return QuoteStatus::TrailingText;
    }
    return QuoteStatus::Ok;
}

// Builds the program one physical line at a time. Every line is interpreted in
// order, and the first rejection ends the parse, so nothing downstream can ever
// observe a partial program.
class LineInterpreter final {
  public:
    explicit LineInterpreter(const std::filesystem::path& commandFile)
        : parentDirectory_(commandFile.parent_path()) {}

    // Interprets one line. Returns the failure to report, or nothing when the
    // line was accepted or skipped.
    [[nodiscard]] auto consume(std::string_view line, std::size_t lineNumber)
        -> std::optional<ParseFailure> {
        const std::string_view text = trimBlank(line);
        if (text.empty() || text.front() == '#') {
            // A blank line and a whole-line comment are not effective commands.
            // There is no inline comment syntax: a '#' anywhere else is token
            // text and normally produces a syntax error.
            return std::nullopt;
        }
        std::size_t cursor = 0;
        const std::string_view verb = takeToken(text, cursor);
        if (quitSeen_) {
            // Checked before the command-count budget, so text after quit is
            // always reported as text after quit.
            return failureAtLine(diagnostic::afterQuit, lineNumber,
                                 "'" + std::string{verb} + "' appears after 'quit'.");
        }
        if (std::optional<ParseFailure> rejected = dispatch(verb, text, cursor, lineNumber)) {
            return rejected;
        }
        if (program_.commands.size() > maxCommands) {
            return failureAtLine(diagnostic::limitExceeded, lineNumber,
                                 "the command file holds more than " + std::to_string(maxCommands) +
                                     " commands.");
        }
        return std::nullopt;
    }

    [[nodiscard]] auto quitSeen() const noexcept -> bool {
        return quitSeen_;
    }

    [[nodiscard]] auto takeProgram() -> Program {
        return std::move(program_);
    }

  private:
    [[nodiscard]] auto dispatch(std::string_view verb, std::string_view text, std::size_t cursor,
                                std::size_t lineNumber) -> std::optional<ParseFailure> {
        if (verb == "open") {
            return dispatchOpen(text, cursor, lineNumber);
        }
        if (verb == "play") {
            return dispatchBare(Verb::Play, verb, text, cursor, lineNumber);
        }
        if (verb == "pause") {
            return dispatchBare(Verb::Pause, verb, text, cursor, lineNumber);
        }
        if (verb == "reload") {
            return dispatchBare(Verb::Reload, verb, text, cursor, lineNumber);
        }
        if (verb == "quit") {
            return dispatchBare(Verb::Quit, verb, text, cursor, lineNumber);
        }
        if (verb == "tick") {
            return dispatchNumber(Verb::Tick, verb, text, cursor, lineNumber);
        }
        if (verb == "seek") {
            return dispatchNumber(Verb::Seek, verb, text, cursor, lineNumber);
        }
        return failureAtLine(diagnostic::unknownVerb, lineNumber,
                             "unknown verb '" + std::string{verb} + "'.");
    }

    // open takes no argument, or exactly one double-quoted path. No argument
    // leaves openPath empty, which is the signal to use the default content
    // root; otherwise the decoded path is resolved against the directory that
    // holds the command file, never against the process working directory.
    [[nodiscard]] auto dispatchOpen(std::string_view text, std::size_t cursor,
                                    std::size_t lineNumber) -> std::optional<ParseFailure> {
        const std::size_t argumentStart = skipBlank(text, cursor);
        if (argumentStart == text.size()) {
            record(Verb::Open, lineNumber, 0, {});
            return std::nullopt;
        }
        const std::string_view argument = text.substr(argumentStart);
        if (argument.front() != '"') {
            return failureAtLine(diagnostic::syntax, lineNumber,
                                 "'open' expects a double-quoted path argument.");
        }
        std::string decoded;
        switch (parseQuotedPath(argument, decoded)) {
        case QuoteStatus::Ok:
            break;
        case QuoteStatus::Unterminated:
            return failureAtLine(diagnostic::syntax, lineNumber,
                                 "'open' has a quoted path with no closing quote.");
        case QuoteStatus::BadEscape:
            return failureAtLine(diagnostic::syntax, lineNumber,
                                 "'open' accepts only \\\" and \\\\ as escapes inside a quoted "
                                 "path.");
        case QuoteStatus::TrailingText:
            return failureAtLine(diagnostic::syntax, lineNumber,
                                 "'open' has text after the closing quote of its path.");
        }
        if (decoded.empty()) {
            return failureAtLine(diagnostic::syntax, lineNumber,
                                 "'open' requires a non-empty path.");
        }
        // The decoded bytes are arbitrary, and building a path from them is the
        // one step here that can fail: on a platform whose native path encoding
        // cannot represent those bytes the conversion throws. Without this guard
        // that exception leaves the parser, so a malformed path argument aborts
        // the host instead of being rejected with a diagnostic, which is the one
        // outcome a command file must never be able to cause.
        std::filesystem::path resolved;
        try {
            resolved = (parentDirectory_ / decoded).lexically_normal();
        } catch (const std::system_error&) {
            return failureAtLine(diagnostic::syntax, lineNumber,
                                 "'open' has a path argument that this system cannot represent.");
        }
        record(Verb::Open, lineNumber, 0, std::move(resolved));
        return std::nullopt;
    }

    [[nodiscard]] auto dispatchBare(Verb code, std::string_view verb, std::string_view text,
                                    std::size_t cursor, std::size_t lineNumber)
        -> std::optional<ParseFailure> {
        if (skipBlank(text, cursor) != text.size()) {
            return failureAtLine(diagnostic::syntax, lineNumber,
                                 "'" + std::string{verb} + "' takes no arguments.");
        }
        record(code, lineNumber, 0, {});
        return std::nullopt;
    }

    [[nodiscard]] auto dispatchNumber(Verb code, std::string_view verb, std::string_view text,
                                      std::size_t cursor, std::size_t lineNumber)
        -> std::optional<ParseFailure> {
        const std::string_view argument = takeToken(text, cursor);
        if (argument.empty() || skipBlank(text, cursor) != text.size()) {
            return failureAtLine(diagnostic::syntax, lineNumber,
                                 "'" + std::string{verb} + "' expects exactly one argument.");
        }
        std::uint64_t number = 0;
        const NumberStatus status = parseDecimal(argument, number);
        if (status == NumberStatus::OutOfRange) {
            return failureAtLine(diagnostic::invalidNumber, lineNumber,
                                 "'" + std::string{verb} + "' found '" + std::string{argument} +
                                     "', which does not fit in an unsigned 64-bit integer.");
        }
        if (status != NumberStatus::Ok) {
            return failureAtLine(diagnostic::invalidNumber, lineNumber,
                                 "'" + std::string{verb} +
                                     "' expects a decimal non-negative "
                                     "integer, but found '" +
                                     std::string{argument} + "'.");
        }
        if (code == Verb::Tick) {
            if (number > maxTickAttempts) {
                return failureAtLine(diagnostic::limitExceeded, lineNumber,
                                     "tick " + std::to_string(number) +
                                         " is above the tick budget of " +
                                         std::to_string(maxTickAttempts) + ".");
            }
            // A tick counts even when it is later suppressed: the budget bounds
            // the work that was asked for, not the work that advanced anything.
            program_.totalTickAttempts += number;
            if (program_.totalTickAttempts > maxTickAttempts) {
                return failureAtLine(diagnostic::limitExceeded, lineNumber,
                                     "the cumulative tick budget of " +
                                         std::to_string(maxTickAttempts) + " is exceeded.");
            }
        } else if (number > maxChartTimeMs) {
            return failureAtLine(diagnostic::limitExceeded, lineNumber,
                                 "seek " + std::to_string(number) +
                                     " is above the maximum chart time of " +
                                     std::to_string(maxChartTimeMs) + ".");
        }
        record(code, lineNumber, number, {});
        return std::nullopt;
    }

    // The single place a command enters the program, so quit can never be
    // recorded without also being remembered.
    void record(Verb code, std::size_t lineNumber, std::uint64_t number,
                std::filesystem::path openPath) {
        Command command;
        command.verb = code;
        command.line = lineNumber;
        command.number = number;
        command.openPath = std::move(openPath);
        program_.commands.push_back(std::move(command));
        if (code == Verb::Quit) {
            quitSeen_ = true;
        }
    }

    std::filesystem::path parentDirectory_;
    Program program_;
    bool quitSeen_{false};
};

} // namespace

auto parseCommandFile(const std::filesystem::path& path) -> std::variant<Program, ParseFailure> {
    std::ifstream stream{path, std::ios::binary};
    if (!stream.is_open()) {
        return failure(diagnostic::fileRead,
                       "cannot read the command file '" + displayPath(path) + "'.");
    }

    // Bounded read: one byte past the limit is enough to prove the file is over
    // it, so an over-large file is refused without reading it into memory.
    std::string content(maxCommandFileBytes + 1U, '\0');
    stream.read(content.data(), static_cast<std::streamsize>(content.size()));
    const std::streamsize received = stream.gcount();
    if (stream.bad() || received < 0) {
        return failure(diagnostic::fileRead,
                       "cannot read the command file '" + displayPath(path) + "'.");
    }
    content.resize(static_cast<std::size_t>(received));
    if (content.size() > maxCommandFileBytes) {
        return failure(diagnostic::limitExceeded, "the command file is larger than the " +
                                                      std::to_string(maxCommandFileBytes) +
                                                      " byte limit.");
    }

    const std::size_t nulOffset = content.find('\0');
    if (nulOffset != std::string::npos) {
        return failureAtLine(diagnostic::syntax, lineAt(content, nulOffset),
                             "the command file contains a NUL byte.");
    }

    std::string_view text{content};
    if (text.starts_with(utf8Bom)) {
        text.remove_prefix(utf8Bom.size());
    }

    LineInterpreter interpreter{path};
    std::size_t position = 0;
    std::size_t lineNumber = 0;
    while (position < text.size()) {
        const std::size_t newline = text.find('\n', position);
        const std::size_t end = newline == std::string_view::npos ? text.size() : newline;
        ++lineNumber;
        if (std::optional<ParseFailure> rejected =
                interpreter.consume(text.substr(position, end - position), lineNumber)) {
            return std::move(*rejected);
        }
        if (newline == std::string_view::npos) {
            break;
        }
        position = newline + 1U;
    }

    if (!interpreter.quitSeen()) {
        return failure(diagnostic::missingQuit, "the command file has no quit command.");
    }
    return interpreter.takeProgram();
}

} // namespace cuexis_reference_host
