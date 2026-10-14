#pragma once

#include "core/Book.h"
#include "core/LendRecord.h"
#include "core/Result.h"
#include "core/ReturnRecord.h"
#include "core/SearchMode.h"

#include <QString>
#include <QVector>

namespace libms {

class BookRepository;
class LoanRepository;

/**
 * @brief 图书馆业务门面。
 *
 * 界面只与本类交互，不直接接触 SQL。本层负责：
 *  - 输入校验（非空、ISBN 规则、数量区间、借阅人姓名规则）；
 *  - 事务边界（借书 = 建记录 + 扣库存，还书 = 建归还记录 + 更新已还数量 + 回冲库存）；
 *  - 返回面向用户的中文错误提示。
 *
 * 失败时返回 Result，由界面决定展示方式；因此本类可以被非 GUI 调用方复用。
 */
class LibraryService
{
public:
    LibraryService(BookRepository &books, LoanRepository &loans);

    // ---- 图书管理 ----------------------------------------------------

    /// 新增图书：校验 ISBN 为 13 位数字且不重复。
    VoidResult addBook(const QString &name, const QString &author, const QString &isbn, int total);

    /**
     * @brief 修改图书。
     * @param book 需带有效 id；@p book.total 为新馆藏总量，
     *             且不得小于当前已借出数量。
     */
    VoidResult updateBook(const Book &book);

    /// 删除图书：存在未归还记录时拒绝删除，避免产生孤儿借阅记录。
    VoidResult removeBook(int bookId);

    /// 检索图书；关键字为空时返回全部。
    QVector<Book> searchBooks(SearchMode mode, const QString &keyword) const;

    /// 列出全部图书。
    QVector<Book> allBooks() const;

    /// 按 ISBN 查询单本图书。
    Result<Book> findBookByIsbn(const QString &isbn) const;

    // ---- 借阅与归还 --------------------------------------------------

    /**
     * @brief 借书。
     *
     * 在一个事务内完成：校验库存 -> 写借阅记录 -> 扣减可借数量。
     * 任一步失败即回滚，不会留下「扣了库存却没有借阅记录」的中间状态。
     */
    VoidResult lendBook(const QString &isbn, const QString &readerId, int count);

    /**
     * @brief 还书。
     *
     * 在一个事务内完成：校验数量 -> 写归还记录 -> 累加已还数量 -> 回冲可借数量。
     * @param count 本次归还数量，必须满足 0 < count <= 未归还数量。
     */
    VoidResult returnBook(int lendRecordId, int count);

    /// 仍有未归还数量的借阅记录。
    QVector<LendRecord> outstandingLoans() const;

    /// 全部借阅记录，最新借出在前。
    QVector<LendRecord> allLoans() const;

    /// 全部归还记录，最新归还在前。
    QVector<ReturnRecord> allReturns() const;

    // ---- 校验规则（public static 便于单元测试直接调用）----------------

    /// ISBN 必须为 13 位数字。
    static bool isValidIsbn(const QString &isbn);

    /// 借阅人姓名：2~20 个汉字或英文字母。
    static bool isValidReaderId(const QString &readerId);

    /// 校验结果对应的中文说明；输入合法时返回空字符串。
    static QString isbnRuleText();
    static QString readerIdRuleText();

private:
    BookRepository &m_books;
    LoanRepository &m_loans;
};

} // namespace libms
