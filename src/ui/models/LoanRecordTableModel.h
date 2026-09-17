#pragma once

#include "core/LendRecord.h"

#include <QAbstractTableModel>
#include <QVector>

namespace libms {

/**
 * @brief 借阅记录的表格模型。
 *
 * 可通过 @p outstandingOnly 只展示尚未归还完的记录，供「归还图书」页使用。
 */
class LoanRecordTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        ColumnReaderId = 0,
        ColumnIsbn,
        ColumnBorrowTime,
        ColumnBorrowCount,
        ColumnReturnedCount,
        ColumnOutstanding,
        ColumnCount,
    };

    explicit LoanRecordTableModel(QObject *parent = nullptr);

    /// 用新的记录列表整体替换现有内容。
    void setRecords(const QVector<LendRecord> &records);

    /// 取第 @p row 行的借阅记录；越界时返回缺省构造的对象。
    LendRecord recordAt(int row) const;

    /// 当前行数。
    int count() const { return m_records.size(); }

private:
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    QVector<LendRecord> m_records;
};

} // namespace libms
