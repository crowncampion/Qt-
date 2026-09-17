#include "data/BookRepository.h"

#include "data/Database.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QVariant>

namespace libms {
namespace {

/// 统一的查询列顺序，保证行 -> Book 的映射与 SQL 一致。
constexpr auto kSelectColumns = "id, book_name, author, ISBN, total, available";

/// 将 QSqlError 转为面向用户的提示。
QString describe(const QSqlError &error)
{
    const QString text = error.databaseText().trimmed();
    return text.isEmpty() ? error.driverText().trimmed() : text;
}

/// 从当前查询行构造 Book。
Book readBook(const QSqlQuery &query)
{
    Book book;
    book.id = query.value(0).toInt();
    book.name = query.value(1).toString();
    book.author = query.value(2).toString();
    book.isbn = query.value(3).toString();
    book.total = query.value(4).toInt();
    book.available = query.value(5).toInt();
    return book;
}

/// MySQL 唯一键冲突的错误码。
constexpr int kDuplicateEntryError = 1062;

} // namespace

BookRepository::BookRepository(Database &database)
    : m_database(database)
{
}

Result<int> BookRepository::insert(const Book &book)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("INSERT INTO book (book_name, author, ISBN, total, available) "
                                 "VALUES (:name, :author, :isbn, :total, :available)"));
    query.bindValue(QStringLiteral(":name"), book.name);
    query.bindValue(QStringLiteral(":author"), book.author);
    query.bindValue(QStringLiteral(":isbn"), book.isbn);
    query.bindValue(QStringLiteral(":total"), book.total);
    // 新书全部可借。
    query.bindValue(QStringLiteral(":available"), book.total);

    if (!query.exec()) {
        if (query.lastError().nativeErrorCode().toInt() == kDuplicateEntryError) {
            return Result<int>::failure(QStringLiteral("ISBN %1 已存在，请勿重复录入").arg(book.isbn));
        }
        return Result<int>::failure(QStringLiteral("新增图书失败：%1").arg(describe(query.lastError())));
    }

    const int newId = query.lastInsertId().toInt();
    if (newId <= 0) {
        return Result<int>::failure(QStringLiteral("新增图书失败：未能获取新书编号"));
    }
    return Result<int>::success(newId);
}

VoidResult BookRepository::update(const Book &book)
{
    QSqlQuery query(m_database.handle());
    // available 随 total 的变化同步调整，但下限为 0、上限为 total，
    // 避免把馆藏总量改小后出现「可借 > 总量」。
    query.prepare(QStringLiteral(
        "UPDATE book SET book_name = :name, author = :author, total = :total, "
        "available = LEAST(GREATEST(total - :borrowed, 0), :total) "
        "WHERE id = :id"));
    query.bindValue(QStringLiteral(":name"), book.name);
    query.bindValue(QStringLiteral(":author"), book.author);
    query.bindValue(QStringLiteral(":total"), book.total);
    query.bindValue(QStringLiteral(":borrowed"), book.borrowedCount());
    query.bindValue(QStringLiteral(":id"), book.id);

    if (!query.exec()) {
        return VoidResult::failure(QStringLiteral("保存图书失败：%1").arg(describe(query.lastError())));
    }
    if (query.numRowsAffected() == 0) {
        return VoidResult::failure(QStringLiteral("保存图书失败：该书已不存在，请刷新后重试"));
    }
    return VoidResult::success();
}

VoidResult BookRepository::remove(int bookId)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("DELETE FROM book WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), bookId);

    if (!query.exec()) {
        return VoidResult::failure(QStringLiteral("删除图书失败：%1").arg(describe(query.lastError())));
    }
    return VoidResult::success();
}

VoidResult BookRepository::adjustAvailable(int bookId, int delta)
{
    QSqlQuery query(m_database.handle());
    // 条件写：只有结果落在 [0, total] 区间内才会命中行。
    query.prepare(QStringLiteral("UPDATE book SET available = available + :delta "
                                 "WHERE id = :id AND available + :delta BETWEEN 0 AND total"));
    query.bindValue(QStringLiteral(":delta"), delta);
    query.bindValue(QStringLiteral(":id"), bookId);

    if (!query.exec()) {
        return VoidResult::failure(QStringLiteral("更新库存失败：%1").arg(describe(query.lastError())));
    }
    if (query.numRowsAffected() == 0) {
        return VoidResult::failure(delta < 0 ? QStringLiteral("可借数量不足，无法完成借出")
                                             : QStringLiteral("可借数量已达馆藏总量，无法继续归还"));
    }
    return VoidResult::success();
}

Result<Book> BookRepository::findById(int bookId)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("SELECT %1 FROM book WHERE id = :id").arg(QLatin1String(kSelectColumns)));
    query.bindValue(QStringLiteral(":id"), bookId);

    if (!query.exec()) {
        return Result<Book>::failure(QStringLiteral("查询图书失败：%1").arg(describe(query.lastError())));
    }
    if (!query.next()) {
        return Result<Book>::failure(QStringLiteral("未找到编号为 %1 的图书").arg(bookId));
    }
    return Result<Book>::success(readBook(query));
}

Result<Book> BookRepository::findByIsbn(const QString &isbn)
{
    QSqlQuery query(m_database.handle());
    query.prepare(
        QStringLiteral("SELECT %1 FROM book WHERE ISBN = :isbn").arg(QLatin1String(kSelectColumns)));
    query.bindValue(QStringLiteral(":isbn"), isbn);

    if (!query.exec()) {
        return Result<Book>::failure(QStringLiteral("查询图书失败：%1").arg(describe(query.lastError())));
    }
    if (!query.next()) {
        return Result<Book>::failure(QStringLiteral("未找到 ISBN 为 %1 的图书").arg(isbn));
    }
    return Result<Book>::success(readBook(query));
}

QVector<Book> BookRepository::search(SearchMode mode, const QString &keyword)
{
    const QString trimmed = keyword.trimmed();

    // 关键字为空时列出全部，等价于原实现的「空条件 → LIKE '%'」。
    if (trimmed.isEmpty()) {
        return findAll();
    }

    QSqlQuery query(m_database.handle());
    if (mode == SearchMode::IsbnExact) {
        query.prepare(QStringLiteral("SELECT %1 FROM book WHERE ISBN = :isbn ORDER BY book_name")
                          .arg(QLatin1String(kSelectColumns)));
        query.bindValue(QStringLiteral(":isbn"), trimmed);
    } else {
        query.prepare(QStringLiteral("SELECT %1 FROM book WHERE book_name LIKE :keyword ORDER BY book_name")
                          .arg(QLatin1String(kSelectColumns)));
        // 转义 LIKE 的通配符，使 % 与 _ 作为普通字符处理。
        QString pattern = trimmed;
        pattern.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
        pattern.replace(QLatin1Char('%'), QLatin1String("\\%"));
        pattern.replace(QLatin1Char('_'), QLatin1String("\\_"));
        query.bindValue(QStringLiteral(":keyword"), QStringLiteral("%%1%").arg(pattern));
    }

    QVector<Book> books;
    if (!query.exec()) {
        qWarning("检索图书失败: %s", qPrintable(describe(query.lastError())));
        return books;
    }
    while (query.next()) {
        books.append(readBook(query));
    }
    return books;
}

QVector<Book> BookRepository::findAll()
{
    QSqlQuery query(m_database.handle());
    QVector<Book> books;
    if (!query.exec(QStringLiteral("SELECT %1 FROM book ORDER BY book_name").arg(QLatin1String(kSelectColumns)))) {
        qWarning("读取图书列表失败: %s", qPrintable(describe(query.lastError())));
        return books;
    }
    while (query.next()) {
        books.append(readBook(query));
    }
    return books;
}

bool BookRepository::existsByIsbn(const QString &isbn, int excludingBookId) const
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM book WHERE ISBN = :isbn AND id <> :id"));
    query.bindValue(QStringLiteral(":isbn"), isbn);
    query.bindValue(QStringLiteral(":id"), excludingBookId);
    if (!query.exec() || !query.next()) {
        return false;
    }
    return query.value(0).toInt() > 0;
}

} // namespace libms
