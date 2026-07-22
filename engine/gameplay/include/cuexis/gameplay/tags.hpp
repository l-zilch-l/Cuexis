#pragma once

//  Gameplay 标签 Component — NoteTag / ElementTag
//  谱面语义通过标签组合区分（音符/元素/装饰物），不建立复杂继承类
//  阶段 1 只使用占位标签，正式判定系统在后续阶段实现

namespace cuexis::gameplay {

struct NoteTag final {};
struct ElementTag final {};

} // namespace cuexis::gameplay
