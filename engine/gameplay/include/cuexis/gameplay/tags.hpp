#pragma once

//  Gameplay tag components - NoteTag / ElementTag
//  Chart semantics are distinguished by tag combinations (note/element/decoration) rather
//  than by deep inheritance hierarchies
//  Phase 1 uses placeholder tags only; the real judgement system arrives in a later phase

namespace cuexis::gameplay {

struct NoteTag final {};
struct ElementTag final {};

} // namespace cuexis::gameplay
