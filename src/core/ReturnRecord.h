#pragma once

#include <QDateTime>
#include <QString>

namespace libms {

/**
 * @brief 归还记录，对应 return_records 表的一行。
 */
struct ReturnRecord
{
    int id = 0;
    /// 关联的借阅记录主键。
    int borrowId = 0;
    QDateTime returnTime;
    int returnCount = 0;

    bool isValid() const { return id > 0; }
};

} // namespace libms
