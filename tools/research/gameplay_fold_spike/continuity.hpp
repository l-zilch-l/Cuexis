// Research spike: continuity grace for handoff Holds and contact-following Sliders.
// This module is not part of the product build.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace spike::continuity {

using Tick = std::int64_t;

enum class Kind : std::uint8_t { Hold, Slider };
enum class Grip : std::uint8_t { Sticky, Handoff };
enum class EventKind : std::uint8_t { Press, Release, Move };
enum class Outcome : std::uint8_t { HeadHit, BodyHit, Break, Miss };
enum class Stage : std::uint8_t { Pending, Follow, Gap, Broken, Settled };

struct Requirement {
    std::uint32_t id = 0;
    Kind kind = Kind::Hold;
    Grip grip = Grip::Handoff;
    std::uint8_t lane = 0;
    Tick anchor = 0;
    Tick end = 0;
    std::uint32_t segments = 1;
};

struct Event {
    Tick t = 0;
    std::uint32_t contact = 0;
    std::uint8_t lane = 0;
    EventKind kind = EventKind::Press;
    std::uint32_t segment = 0;
};

struct Config {
    Tick goodUs = 120'000;
    Tick holdGraceUs = 60'000;
};

struct Fact {
    Tick t = 0;
    std::uint32_t req = 0;
    Outcome outcome = Outcome::HeadHit;
    std::uint8_t phase = 0;
};

struct ItemSummary {
    std::uint32_t req = 0;
    Stage stage = Stage::Pending;
    Tick releasedAt = 0;
    std::uint32_t contact = 0;
    std::uint32_t segment = 0;
    std::uint32_t moves = 0;
    bool headEmitted = false;
    bool breakEmitted = false;
};

struct Result {
    std::vector<Fact> facts;
    std::vector<ItemSummary> items;
    std::size_t strays = 0;
    std::uint64_t digest = 0;
    std::uint64_t judgementDigest = 0;
};

class Session {
  public:
    Session(const Config& config, const std::vector<Requirement>& requirements);
    ~Session();
    Session(Session&&) noexcept;
    Session& operator=(Session&&) noexcept;
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    void apply(const Event& event);
    void finish();
    std::vector<std::uint8_t> snapshot() const;
    void restore(const std::vector<std::uint8_t>& bytes);
    Result result() const;

  private:
    struct Impl;
    Impl* impl_ = nullptr;
};

Result run(const Config& config, const std::vector<Requirement>& requirements,
           const std::vector<Event>& events);
std::string describe(const Result& result);

} // namespace spike::continuity
