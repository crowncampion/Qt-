#include "service/LibraryService.h"

#include "core/TimeUtil.h"
#include "data/BookRepository.h"
#include "data/Database.h"
#include "data/LoanRepository.h"

#include <QLoggingCategory>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>

namespace libms {
namespace {

Q_LOGGING_CATEGORY(lcService, "libms.service")

/// 馆藏总量上限，与界面输入控件的上限保持一致。
constexpr int kMaxCopies = 999;

/// 单次借阅/归还的数量上限。
constexpr int kMaxTransactionCount = 999;

/// 书名/作者的长度上限，与数据库列宽一致。
constexpr int kMaxNameLength = 50;

/// 把 QSqlError 转为可展示文本。
QString describe(const QSqlError &error)
{
    const QString text = error.databaseText().trimmed();
    return text.isEmpty() ? error.driverText().trimmed() : text;
}

/// 事务回滚并返回失败结果。
VoidResult rollbackWith(QSqlDatabase &db, const QString &message)
{
    db.rollback();
    return VoidResult::failure(message);
}

} // namespace

LibraryService::LibraryService(BookRepository &books, LoanRepository &loans)
    : m_books(books)
    , m_loans(loans)
{
}

// ---- 校验规则 ---------------------------------------------------------

bool LibraryService::isValidIsbn(const QString &isbn)
{
    static const QRegularExpression pattern(QStringLiteral("^\\d{13}$"));
    return pattern.match(isbn.trimmed()).hasMatch();
}

bool LibraryService::isValidReaderId(const QString &readerId)
{
    // 汉字 + 英文字母，2~20 个字符；与原实现的校验规则一致。
    static const QRegularExpression pattern(QStringLiteral("^[\\x{4e00}-\\x{9fa5}A-Za-z]{2,20}$"));
    return pattern.match(readerId.trimmed()).hasMatch();
}

QString LibraryService::isbnRuleText()
{
    return QStringLiteral("ISBN 必须是 13 位数字");
}

QString LibraryService::readerIdRuleText()
{
    return QStringLiteral("借阅人姓名只能是 2~20 个汉字或英文字母");
}

// ---- 图书管理 ---------------------------------------------------------

VoidResult LibraryService::addBook(const QString &name, const QString &author, const QString &isbn, int total)
{
    const QString trimmedName = name.trimmed();
    const QString trimmedAuthor = author.trimmed();
    const QString trimmedIsbn = isbn.trimmed();

    QStringList problems;
    if (trimmedName.isEmpty()) {
        problems.append(QStringLiteral("书名不能为空"));
    } else if (trimmedName.length() > kMaxNameLength) {
        problems.append(QStringLiteral("书名不能超过 %1 个字符").arg(kMaxNameLength));
    }
    if (trimmedAuthor.isEmpty()) {
        problems.append(QStringLiteral("作者不能为空"));
    } else if (trimmedAuthor.length() > kMaxNameLength) {
        problems.append(QStringLiteral("作者不能超过 %1 个字符").arg(kMaxNameLength));
    }
    if (!isValidIsbn(trimmedIsbn)) {
        problems.append(isbnRuleText());
    }
    if (total <= 0 || total > kMaxCopies) {
        problems.append(QStringLiteral("馆藏总量必须在 1 ~ %1 之间").arg(kMaxCopies));
    }
    if (!problems.isEmpty()) {
        return VoidResult::failure(problems.join(QLatin1Char('\n')));
    }

    if (m_books.existsByIsbn(trimmedIsbn)) {
        return VoidResult::failure(QStringLiteral("ISBN %1 已存在，请勿重复录入").arg(trimmedIsbn));
    }

    Book book;
    book.name = trimmedName;
    book.author = trimmedAuthor;
    book.isbn = trimmedIsbn;
    book.total = total;
    book.available = total;

    const Result<int> inserted = m_books.insert(book);
    if (!inserted) {
        return VoidResult::failure(inserted.error());
    }

    qCInfo(lcService) << "新增图书" << trimmedName << "id=" << inserted.value();
    return VoidResult::success();
}

VoidResult LibraryService::updateBook(const Book &book)
{
    const QString trimmedName = book.name.trimmed();
    const QString trimmedAuthor = book.author.trimmed();

    if (book.id <= 0) {
        return VoidResult::failure(QStringLiteral("要修改的图书不存在，请刷新后重试"));
    }
    if (trimmedName.isEmpty()) {
        return VoidResult::failure(QStringLiteral("书名不能为空"));
    }
    if (trimmedName.length() > kMaxNameLength) {
        return VoidResult::failure(QStringLiteral("书名不能超过 %1 个字符").arg(kMaxNameLength));
    }
    if (trimmedAuthor.isEmpty()) {
        return VoidResult::failure(QStringLiteral("作者不能为空"));
    }
    if (book.total <= 0 || book.total > kMaxCopies) {
        return VoidResult::failure(QStringLiteral("馆藏总量必须在 1 ~ %1 之间").arg(kMaxCopies));
    }

    const Result<Book> existing = m_books.findById(book.id);
    if (!existing) {
        return VoidResult::failure(existing.error());
    }

    // 已借出的数量不可能超过馆藏总量，否则数据自相矛盾。
    const int borrowed = existing.value().borrowedCount();
    if (book.total < borrowed) {
        return VoidResult::failure(
            QStringLiteral("馆藏总量不能小于当前已借出的 %1 本").arg(borrowed));
    }

    Book updated = book;
    updated.name = trimmedName;
    updated.author = trimmedAuthor;
    updated.isbn = existing.value().isbn;
    updated.available = book.total - borrowed;

    const VoidResult saved = m_books.update(updated);
    if (!saved) {
        return saved;
    }

    qCInfo(lcService) << "修改图书 id=" << book.id << "总量=" << book.total;
    return VoidResult::success();
}

VoidResult LibraryService::removeBook(int bookId)
{
    if (bookId <= 0) {
        return VoidResult::failure(QStringLiteral("请先选择要删除的图书"));
    }

    const Result<Book> existing = m_books.findById(bookId);
    if (!existing) {
        return VoidResult::failure(existing.error());
    }

    // 有未归还记录时拒绝删除，避免借阅/归还记录失去对应的图书信息。
    const int outstanding = m_loans.outstandingCountForIsbn(existing.value().isbn);
    if (outstanding > 0) {
        return VoidResult::failure(
            QStringLiteral("《%1》尚有 %2 本未归还，无法删除").arg(existing.value().name).arg(outstanding));
    }

    const VoidResult removed = m_books.remove(bookId);
    if (!removed) {
        return removed;
    }

    qCInfo(lcService) << "删除图书" << existing.value().name << "id=" << bookId;
    return VoidResult::success();
}

QVector<Book> LibraryService::searchBooks(SearchMode mode, const QString &keyword) const
{
    return m_books.search(mode, keyword);
}

QVector<Book> LibraryService::allBooks() const
{
    return m_books.findAll();
}

Result<Book> LibraryService::findBookByIsbn(const QString &isbn) const
{
    return m_books.findByIsbn(isbn.trimmed());
}

// ---- 借阅与归还 -------------------------------------------------------

VoidResult LibraryService::lendBook(const QString &isbn, const QString &readerId, int count)
{
    const QString trimmedIsbn = isbn.trimmed();
    const QString trimmedReader = readerId.trimmed();

    if (trimmedIsbn.isEmpty() || trimmedReader.isEmpty()) {
        return VoidResult::failure(QStringLiteral("借书信息不完整：请填写图书 ISBN 与借阅人"));
    }
    if (!isValidIsbn(trimmedIsbn)) {
        return VoidResult::failure(isbnRuleText());
    }
    if (!isValidReaderId(trimmedReader)) {
        return VoidResult::failure(readerIdRuleText());
    }
    if (count <= 0 || count > kMaxTransactionCount) {
        return VoidResult::failure(QStringLiteral("借出数量必须在 1 ~ %1 之间").arg(kMaxTransactionCount));
    }

    const Result<Book> book = m_books.findByIsbn(trimmedIsbn);
    if (!book) {
        return VoidResult::failure(book.error());
    }
    if (book.value().available < count) {
        return VoidResult::failure(QStringLiteral("《%1》当前仅有 %2 本可借，无法借出 %3 本")
                                       .arg(book.value().name)
                                       .arg(book.value().available)
                                       .arg(count));
    }

    Database &database = m_books.database();
    QSqlDatabase db = database.handle();
    if (!db.transaction()) {
        return VoidResult::failure(QStringLiteral("无法开启事务：%1").arg(describe(db.lastError())));
    }

    LendRecord record;
    record.readerId = trimmedReader;
    record.isbn = trimmedIsbn;
    record.borrowTime = TimeUtil::now();
    record.borrowCount = count;
    record.returnedCount = 0;

    const Result<int> lendId = m_loans.insertLendRecord(record);
    if (!lendId) {
        return rollbackWith(db, lendId.error());
    }

    const VoidResult adjusted = m_books.adjustAvailable(book.value().id, -count);
    if (!adjusted) {
        return rollbackWith(db, adjusted.error());
    }

    if (!db.commit()) {
        return rollbackWith(db, QStringLiteral("提交借阅事务失败：%1").arg(describe(db.lastError())));
    }

    qCInfo(lcService) << "借出" << book.value().name << "x" << count << "给" << trimmedReader;
    return VoidResult::success();
}

VoidResult LibraryService::returnBook(int lendRecordId, int count)
{
    if (lendRecordId <= 0) {
        return VoidResult::failure(QStringLiteral("请先选择要归还的借阅记录"));
    }

    const Result<LendRecord> record = m_loans.findLendRecordById(lendRecordId);
    if (!record) {
        return VoidResult::failure(record.error());
    }

    const int outstanding = record.value().outstandingCount();
    if (outstanding <= 0) {
        return VoidResult::failure(QStringLiteral("该借阅记录已全部归还，无需再次归还"));
    }
    if (count <= 0 || count > outstanding) {
        return VoidResult::failure(
            QStringLiteral("归还数量必须在 1 ~ %1 之间").arg(outstanding));
    }

    // 图书可能已被删除；此时只更新借阅记录，不再回冲库存。
    const Result<Book> book = m_books.findByIsbn(record.value().isbn);
    if (!book) {
        qCWarning(lcService) << "归还时未找到对应图书，仅更新借阅记录: isbn=" << record.value().isbn;
    }

    Database &database = m_books.database();
    QSqlDatabase db = database.handle();
    if (!db.transaction()) {
        return VoidResult::failure(QStringLiteral("无法开启事务：%1").arg(describe(db.lastError())));
    }

    ReturnRecord returnRecord;
    returnRecord.borrowId = record.value().id;
    returnRecord.returnTime = TimeUtil::now();
    returnRecord.returnCount = count;

    const Result<int> returnId = m_loans.insertReturnRecord(returnRecord);
    if (!returnId) {
        return rollbackWith(db, returnId.error());
    }

    const VoidResult countUpdated =
        m_loans.updateReturnedCount(record.value().id, record.value().returnedCount + count);
    if (!countUpdated) {
        return rollbackWith(db, countUpdated.error());
    }

    if (book) {
        const VoidResult restored = m_books.adjustAvailable(book.value().id, count);
        if (!restored) {
            return rollbackWith(db, restored.error());
        }
    }

    if (!db.commit()) {
        return rollbackWith(db, QStringLiteral("提交归还事务失败：%1").arg(describe(db.lastError())));
    }

    qCInfo(lcService) << "归还借阅记录" << record.value().id << "x" << count;
    return VoidResult::success();
}

QVector<LendRecord> LibraryService::outstandingLoans() const
{
    return m_loans.findOutstandingLendRecords();
}

QVector<LendRecord> LibraryService::allLoans() const
{
    return m_loans.findAllLendRecords();
}

QVector<ReturnRecord> LibraryService::allReturns() const
{
    return m_loans.findAllReturnRecords();
}

} // namespace libms
