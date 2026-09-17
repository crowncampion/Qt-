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
    int outstandingCount() const { return borrowCount - returnedCount; }

    bool isFullyReturned() const { return outstandingCount() <= 0; }

    bool isValid() const { return id > 0; }
};

} // namespace libms
