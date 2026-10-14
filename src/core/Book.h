#pragma once

#include <QString>

namespace libms {

/**
 * @brief 图书领域对象，对应 book 表的一行。
 *
 * 只承载数据，不含 SQL 与界面逻辑。
 */
struct Book
{
    /// 主键；未入库时为 0。
    int id = 0;
    QString name;
    QString author;
    /// 13 位数字，全库唯一，业务上作为图书的自然主键。
    QString isbn;
    /// 馆藏总量。
    int total = 0;
    /// 当前可借数量，恒满足 0 <= available <= total。
    int available = 0;

    /// 已借出数量。
    int borrowedCount() const { return total - available; }

    bool isValid() const { return id > 0; }
};

} // namespace libms
