#pragma once

#include "core/LendRecord.h"
#include "core/ReturnRecord.h"

#include <QAbstractTableModel>
#include <QHash>
#include <QVector>

namespace libms {

/**
 * @brief 归还记录的表格模型。
 *
 * return_records 只保存 borrow_id，因此需要借阅记录做一次内存关联，
 * 才能展示「借阅人 / ISBN / 借出时间 / 借出数量」等上下文信息。
 */
class ReturnRecordTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        ColumnReturnTime = 0,
        ColumnReaderId,
        ColumnIsbn,
        ColumnBorrowTime,
        ColumnBorrowCount,
        ColumnReturnCount,
        ColumnCount,
    };

    explicit ReturnRecordTableModel(QObject *parent = nullptr);

    /**
     * @brief 设置数据。
     * @param records 归还记录列表。
     * @param lendRecords 用于关联上下文的借阅记录列表。
     */
    void setRecords(const QVector<ReturnRecord> &records, const QVector<LendRecord> &lendRecords);

    /// 当前行数。
    int count() const { return m_records.size(); }

private:
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    QVector<ReturnRecord> m_records;
    /// borrow_id -> 借阅记录，用于补充展示信息。
    QHash<int, LendRecord> m_lendRecordsById;
};

} // namespace libms
