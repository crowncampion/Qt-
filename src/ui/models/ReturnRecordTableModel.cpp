#include "ui/models/ReturnRecordTableModel.h"

#include "core/TimeUtil.h"

namespace libms {

ReturnRecordTableModel::ReturnRecordTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void ReturnRecordTableModel::setRecords(const QVector<ReturnRecord> &records,
                                       const QVector<LendRecord> &lendRecords)
{
    beginResetModel();
    m_records = records;

    m_lendRecordsById.clear();
    m_lendRecordsById.reserve(lendRecords.size());
    for (const LendRecord &lendRecord : lendRecords) {
        m_lendRecordsById.insert(lendRecord.id, lendRecord);
    }

    endResetModel();
}

int ReturnRecordTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_records.size();
}

int ReturnRecordTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant ReturnRecordTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_records.size()) {
        return {};
    }

    const ReturnRecord &record = m_records.at(index.row());
    // 借阅记录可能因历史数据缺失而不存在，此时相关列显示为「-」。
    const LendRecord lendRecord = m_lendRecordsById.value(record.borrowId);
    const bool hasLendRecord = m_lendRecordsById.contains(record.borrowId);

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case ColumnReturnTime:
            return TimeUtil::format(record.returnTime);
        case ColumnReaderId:
            return hasLendRecord ? QVariant(lendRecord.readerId) : QVariant(QStringLiteral("-"));
        case ColumnIsbn:
            return hasLendRecord ? QVariant(lendRecord.isbn) : QVariant(QStringLiteral("-"));
        case ColumnBorrowTime:
            return hasLendRecord ? QVariant(TimeUtil::format(lendRecord.borrowTime))
                                 : QVariant(QStringLiteral("-"));
        case ColumnBorrowCount:
            return hasLendRecord ? QVariant(lendRecord.borrowCount) : QVariant(QStringLiteral("-"));
        case ColumnReturnCount:
            return record.returnCount;
        default:
            break;
        }
        return {};

    case Qt::TextAlignmentRole:
        switch (index.column()) {
        case ColumnBorrowCount:
        case ColumnReturnCount:
            return QVariant(Qt::AlignCenter);
        default:
            return QVariant(Qt::AlignVCenter | Qt::AlignLeft);
        }

    case Qt::ToolTipRole:
        if (!hasLendRecord) {
            return QStringLiteral("归还时间：%1\n归还数量：%2\n（原始借阅记录已不存在）")
                .arg(TimeUtil::format(record.returnTime))
                .arg(record.returnCount);
        }
        return QStringLiteral("借阅人：%1\nISBN：%2\n借出时间：%3\n借出 %4 本\n归还时间：%5\n归还 %6 本")
            .arg(lendRecord.readerId, lendRecord.isbn, TimeUtil::format(lendRecord.borrowTime))
            .arg(lendRecord.borrowCount)
            .arg(TimeUtil::format(record.returnTime))
            .arg(record.returnCount);

    default:
        return {};
    }
}

QVariant ReturnRecordTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
        return {};
    }

    switch (section) {
    case ColumnReturnTime:
        return QStringLiteral("归还时间");
    case ColumnReaderId:
        return QStringLiteral("借阅人");
    case ColumnIsbn:
        return QStringLiteral("ISBN");
    case ColumnBorrowTime:
        return QStringLiteral("借出时间");
    case ColumnBorrowCount:
        return QStringLiteral("借出数量");
    case ColumnReturnCount:
        return QStringLiteral("归还数量");
    default:
        return {};
    }
}

} // namespace libms
