#pragma once

#include "core/LendRecord.h"
#include "core/Result.h"
#include "core/ReturnRecord.h"

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>

namespace libms {

class Database;

/**
 * @brief lend_records 与 return_records 两表的数据访问对象。
 *
 * 借阅与归还在业务上是一体的（归还记录通过 borrow_id 外键指向借阅记录），
 * 因此放在同一个 DAO 中，便于在一个事务里维护两表的一致性。
 */
class LoanRepository
{
public:
    explicit LoanRepository(Database &database);

    /// 新增一条借阅记录。
    Result<int> insertLendRecord(const LendRecord &record);

    /// 新增一条归还记录。
    Result<int> insertReturnRecord(const ReturnRecord &record);

    /// 将借阅记录的已归还数量设为 @p returnedCount。
    VoidResult updateReturnedCount(int lendRecordId, int returnedCount);

    /// 按主键查询借阅记录。
    Result<LendRecord> findLendRecordById(int lendRecordId);

    /// 列出全部借阅记录，最新借出在前。
    QVector<LendRecord> findAllLendRecords();

    /// 列出仍有未归还数量的借阅记录，最新借出在前。
    QVector<LendRecord> findOutstandingLendRecords();

    /// 列出全部归还记录，最新归还在前。
    QVector<ReturnRecord> findAllReturnRecords();

    /// 汇总某本书当前未归还的总数量（用于删除图书前的校验）。
    int outstandingCountForIsbn(const QString &isbn) const;

    /// 该书是否存在未归还记录。
    bool hasOutstandingLoan(const QString &isbn) const;

private:
    Database &m_database;
};

} // namespace libms
