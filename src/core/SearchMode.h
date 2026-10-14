#pragma once

namespace libms {

/**
 * @brief 图书检索方式。
 *
 * 原实现用「滚轮切换输入框」隐式表达检索模式，用户难以察觉；
 * 这里改为显式枚举，由下拉框选择。
 */
enum class SearchMode
{
    /// 按书名模糊匹配（LIKE %keyword%）。
    NameKeyword,
    /// 按 ISBN 精确匹配。
    IsbnExact,
};

} // namespace libms
