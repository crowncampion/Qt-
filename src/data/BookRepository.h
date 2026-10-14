#pragma once

#include "core/Book.h"
#include "core/Result.h"
#include "core/SearchMode.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace libms {

class Database;

/**
 * @brief book 表的数据访问对象（DAO）。
 *
 * 只负责 SQL 与「行 -> Book」的映射，不做业务校验、不弹窗、不接触界面类型。
 * 所有写操作都使用预处理语句绑定参数，避免 SQL 注入。
 */
class BookRepository
{
public:
    explicit BookRepository(Database &database);

    /**
     * @brief 新增图书。
     * @return 成功时返回新书的主键；ISBN 重复时返回失败。
     */
    Result<int> insert(const Book &book);

    /// 更新书名、作者与馆藏总量（不改变 ISBN）。
    VoidResult update(const Book &book);

    /// 按主键删除；调用方需先确认该书没有未归还记录。
    VoidResult remove(int bookId);

    /**
     * @brief 原子调整可借数量。
     *
     * 借出时 @p delta 为负、归还时为正。
     * 条件写（available + delta BETWEEN 0 AND total）保证并发下不会超借或超还；
     * 影响行数为 0 表示库存不足或图书不存在。
     */
    VoidResult adjustAvailable(int bookId, int delta);

    /// 按主键查询。
    Result<Book> findById(int bookId);

    /// 按 ISBN 精确查询。
    Result<Book> findByIsbn(const QString &isbn);

    /// 按 @p mode 检索；关键字模式使用 LIKE 模糊匹配。
    QVector<Book> search(SearchMode mode, const QString &keyword);

    /// 列出全部图书，按书名排序。
    QVector<Book> findAll();

    /// 判断 ISBN 是否已被占用；@p excludingBookId 用于编辑场景排除自身。
    bool existsByIsbn(const QString &isbn, int excludingBookId = 0) const;

    /// 底层连接，供业务层划分事务边界使用（业务层不直接执行 SQL）。
    Database &database() const { return m_database; }

private:
    Database &m_database;
};

} // namespace libms
