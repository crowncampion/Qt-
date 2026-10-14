#include "ui/models/LoanRecordTableModel.h"

#include "core/TimeUtil.h"

namespace libms {

LoanRecordTableModel::LoanRecordTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void LoanRecordTableModel::setRecords(const QVector<LendRecord> &records)
{
    beginResetModel();
    m_records = records;
    endResetModel();
}

LendRecord LoanRecordTableModel::recordAt(int row) const
{
    if (row < 0 || row >= m_records.size()) {
        return LendRecord{};
    }
    return m_records.at(row);
}

int LoanRecordTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_records.size();
}

int LoanRecordTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant LoanRecordTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_records.size()) {
        return {};
    }

    const LendRecord &record = m_records.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case ColumnReaderId:
            return record.readerId;
        case ColumnIsbn:
            return record.isbn;
        case ColumnBorrowTime:
            return TimeUtil::format(record.borrowTime);
        case ColumnBorrowCount:
            return record.borrowCount;
        case ColumnReturnedCount:
            return record.returnedCount;
        case ColumnOutstanding:
            return record.outstandingCount();
        default:
            break;
        }
        return {};

    case Qt::TextAlignmentRole:
        switch (index.column()) {
        case ColumnBorrowCount:
        case ColumnReturnedCount:
        case ColumnOutstanding:
            return QVariant(Qt::AlignCenter);
        default:
            return QVariant(Qt::AlignVCenter | Qt::AlignLeft);
        }

    case Qt::ToolTipRole:
        return QStringLiteral("借阅人：%1\nISBN：%2\n借出时间：%3\n借出 %4 本，已还 %5 本，未还 %6 本")
            .arg(record.readerId, record.isbn, TimeUtil::format(record.borrowTime))
            .arg(record.borrowCount)
            .arg(record.returnedCount)
            .arg(record.outstandingCount());

    default:
        return {};
    }
}

QVariant LoanRecordTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
        return {};
    }

    switch (section) {
    case ColumnReaderId:
        return QStringLiteral("借阅人");
    case ColumnIsbn:
        return QStringLiteral("ISBN");
    case ColumnBorrowTime:
        return QStringLiteral("借出时间");
    case ColumnBorrowCount:
        return QStringLiteral("借出数量");
    case ColumnReturnedCount:
        return QStringLiteral("已还数量");
    case ColumnOutstanding:
        return QStringLiteral("未还数量");
    default:
        return {};
    }
}

} // namespace libms
