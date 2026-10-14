#pragma once

#include <QDateTime>
#include <QString>

namespace libms {

/**
 * @brief 借阅记录，对应 lend_records 表的一行。
 */
struct LendRecord
{
    int id = 0;
    QString readerId;
    QString isbn;
    QDateTime borrowTime;
    /// 本次借出数量。
    int borrowCount = 0;
    /// 累计已归还数量，恒满足 0 <= returnedCount <= borrowCount。
    int returnedCount = 0;

    /// 尚未归还的数量。
    ///
    /// 正常情况下不会为负：数据库有 chk_lend_counts 约束
    /// （returned_count BETWEEN 0 AND borrow_count），且归还前会校验数量。
    int outstandingCount() const { return borrowCount - returnedCount; }

    /// 是否已全部归还。
    ///
    /// 防御：用 <= 0 而非 == 0。欠数变成负数说明历史数据异常（约束被绕过、
    /// 手工改库、旧数据迁移），此时仍判定为「已还清」——否则这条记录会
    /// 永远还不掉，而图书本身是完好的。异常由 LoanRepository 读取时记日志。
    bool isFullyReturned() const { return outstandingCount() <= 0; }

    bool isValid() const { return id > 0; }
};

} // namespace libms
