#pragma once

namespace libms {

/**
 * @brief 主窗口中各功能页的标识。
 *
 * 原实现用 setCurrentIndex(1..5) 的裸数字寻址，一旦插入页面就会整体错位
 * （历史上「归还图书」确实因此打开了借阅页）。改用具名枚举后由 MainWindow 统一映射。
 */
enum class PageId
{
    Home = 0,
    AddBook,
    ManageBooks,
    LendBook,
    ReturnBook,
    Overview,
};

} // namespace libms
