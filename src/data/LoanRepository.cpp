#include "data/LoanRepository.h"

#include "core/TimeUtil.h"
#include "data/Database.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace libms {
namespace {

/// 统一的借阅记录列顺序，保证行 -> LendRecord 的映射与 SQL 一致。
constexpr auto kLendColumns = "id, reader_id, isbn, borrow_time, borrow_count, returned_count";

QString describe(const QSqlError &error)
{
    const QString text = error.databaseText().trimmed();
    return text.isEmpty() ? error.driverText().trimmed() : text;
}

LendRecord readLendRecord(const QSqlQuery &query)
{
    LendRecord record;
    record.id = query.value(0).toInt();
    record.readerId = query.value(1).toString();
    record.isbn = query.value(2).toString();
    record.borrowTime = TimeUtil::parse(query.value(3).toString());
    record.borrowCount = query.value(4).toInt();
    record.returnedCount = query.value(5).toInt();
    return record;
}

ReturnRecord readReturnRecord(const QSqlQuery &query)
{
    ReturnRecord record;
    record.id = query.value(0).toInt();
    record.borrowId = query.value(1).toInt();
    record.returnTime = TimeUtil::parse(query.value(2).toString());
    record.returnCount = query.value(3).toInt();
    return record;
}

} // namespace

LoanRepository::LoanRepository(Database &database)
    : m_database(database)
{
}

Result<int> LoanRepository::insertLendRecord(const LendRecord &record)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "INSERT INTO lend_records (reader_id, isbn, borrow_time, borrow_count, returned_count) "
        "VALUES (:reader, :isbn, :time, :count, :returned)"));
    query.bindValue(QStringLiteral(":reader"), record.readerId);
    query.bindValue(QStringLiteral(":isbn"), record.isbn);
    query.bindValue(QStringLiteral(":time"), TimeUtil::format(record.borrowTime));
    query.bindValue(QStringLiteral(":count"), record.borrowCount);
    query.bindValue(QStringLiteral(":returned"), record.returnedCount);

    if (!query.exec()) {
        return Result<int>::failure(QStringLiteral("写入借阅记录失败：%1").arg(describe(query.lastError())));
    }

    const int newId = query.lastInsertId().toInt();
    if (newId <= 0) {
        return Result<int>::failure(QStringLiteral("写入借阅记录失败：未能获取记录编号"));
    }
    return Result<int>::success(newId);
}

Result<int> LoanRepository::insertReturnRecord(const ReturnRecord &record)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral(
        "INSERT INTO return_records (borrow_id, return_time, return_count) "
        "VALUES (:borrowId, :time, :count)"));
    query.bindValue(QStringLiteral(":borrowId"), record.borrowId);
    query.bindValue(QStringLiteral(":time"), TimeUtil::format(record.returnTime));
    query.bindValue(QStringLiteral(":count"), record.returnCount);

    if (!query.exec()) {
        return Result<int>::failure(QStringLiteral("写入归还记录失败：%1").arg(describe(query.lastError())));
    }

    const int newId = query.lastInsertId().toInt();
    if (newId <= 0) {
        return Result<int>::failure(QStringLiteral("写入归还记录失败：未能获取记录编号"));
    }
    return Result<int>::success(newId);
}

VoidResult LoanRepository::updateReturnedCount(int lendRecordId, int returnedCount)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("UPDATE lend_records SET returned_count = :returned WHERE id = :id"));
    query.bindValue(QStringLiteral(":returned"), returnedCount);
    query.bindValue(QStringLiteral(":id"), lendRecordId);

    if (!query.exec()) {
        return VoidResult::failure(QStringLiteral("更新借阅记录失败：%1").arg(describe(query.lastError())));
    }
    if (query.numRowsAffected() == 0) {
        return VoidResult::failure(QStringLiteral("更新借阅记录失败：该借阅记录已不存在"));
    }
    return VoidResult::success();
}

Result<LendRecord> LoanRepository::findLendRecordById(int lendRecordId)
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("SELECT %1 FROM lend_records WHERE id = :id").arg(QLatin1String(kLendColumns)));
    query.bindValue(QStringLiteral(":id"), lendRecordId);

    if (!query.exec()) {
        return Result<LendRecord>::failure(
            QStringLiteral("查询借阅记录失败：%1").arg(describe(query.lastError())));
    }
    if (!query.next()) {
        return Result<LendRecord>::failure(QStringLiteral("未找到编号为 %1 的借阅记录").arg(lendRecordId));
    }
    return Result<LendRecord>::success(readLendRecord(query));
}

QVector<LendRecord> LoanRepository::findAllLendRecords()
{
    QSqlQuery query(m_database.handle());
    QVector<LendRecord> records;
    if (!query.exec(QStringLiteral("SELECT %1 FROM lend_records ORDER BY borrow_time DESC, id DESC")
                        .arg(QLatin1String(kLendColumns)))) {
        qWarning("读取借阅记录失败: %s", qPrintable(describe(query.lastError())));
        return records;
    }
    while (query.next()) {
        records.append(readLendRecord(query));
    }
    return records;
}

QVector<LendRecord> LoanRepository::findOutstandingLendRecords()
{
    QSqlQuery query(m_database.handle());
    QVector<LendRecord> records;
    // borrow_count > returned_count 即「尚有未归还数量」。
    if (!query.exec(QStringLiteral("SELECT %1 FROM lend_records WHERE borrow_count > returned_count "
                                   "ORDER BY borrow_time DESC, id DESC")
                        .arg(QLatin1String(kLendColumns)))) {
        qWarning("读取未归还记录失败: %s", qPrintable(describe(query.lastError())));
        return records;
    }
    while (query.next()) {
        records.append(readLendRecord(query));
    }
    return records;
}

QVector<ReturnRecord> LoanRepository::findAllReturnRecords()
{
    QSqlQuery query(m_database.handle());
    QVector<ReturnRecord> records;
    if (!query.exec(QStringLiteral("SELECT id, borrow_id, return_time, return_count FROM return_records "
                                   "ORDER BY return_time DESC, id DESC"))) {
        qWarning("读取归还记录失败: %s", qPrintable(describe(query.lastError())));
        return records;
    }
    while (query.next()) {
        records.append(readReturnRecord(query));
    }
    return records;
}

int LoanRepository::outstandingCountForIsbn(const QString &isbn) const
{
    QSqlQuery query(m_database.handle());
    query.prepare(QStringLiteral("SELECT COALESCE(SUM(borrow_count - returned_count), 0) "
                                 "FROM lend_records WHERE isbn = :isbn AND borrow_count > returned_count"));
    query.bindValue(QStringLiteral(":isbn"), isbn);

    if (!query.exec() || !query.next()) {
        qWarning("统计未归还数量失败: %s", qPrintable(describe(query.lastError())));
        return 0;
    }
    return query.value(0).toInt();
}

bool LoanRepository::hasOutstandingLoan(const QString &isbn) const
{
    return outstandingCountForIsbn(isbn) > 0;
}

} // namespace libms
