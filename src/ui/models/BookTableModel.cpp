#include "ui/models/BookTableModel.h"

#include "core/TimeUtil.h"

#include <QBrush>
#include <QColor>

namespace libms {
namespace {

/// 库存不足时的提示色。
const QColor kOutOfStockColor(0xB0, 0x2A, 0x2A);

} // namespace

BookTableModel::BookTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void BookTableModel::setBooks(const QVector<Book> &books)
{
    beginResetModel();
    m_books = books;
    endResetModel();
}

Book BookTableModel::bookAt(int row) const
{
    if (row < 0 || row >= m_books.size()) {
        return Book{};
    }
    return m_books.at(row);
}

int BookTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_books.size();
}

int BookTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant BookTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_books.size()) {
        return {};
    }

    const Book &book = m_books.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case ColumnIsbn:
            return book.isbn;
        case ColumnName:
            return book.name;
        case ColumnAuthor:
            return book.author;
        case ColumnTotal:
            return book.total;
        case ColumnAvailable:
            return book.available;
        case ColumnBorrowed:
            return book.borrowedCount();
        default:
            break;
        }
        return {};

    case Qt::TextAlignmentRole:
        switch (index.column()) {
        case ColumnTotal:
        case ColumnAvailable:
        case ColumnBorrowed:
            return QVariant(Qt::AlignCenter);
        default:
            return QVariant(Qt::AlignVCenter | Qt::AlignLeft);
        }

    case Qt::ToolTipRole:
        return QStringLiteral("%1\n作者：%2\nISBN：%3\n馆藏：%4 本，可借：%5 本")
            .arg(book.name, book.author, book.isbn)
            .arg(book.total)
            .arg(book.available);

    case Qt::ForegroundRole:
        // 全部借出时用醒目颜色标注，便于管理员一眼发现。
        if (index.column() == ColumnAvailable && book.available <= 0) {
            return QBrush(kOutOfStockColor);
        }
        return {};

    default:
        return {};
    }
}

QVariant BookTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
        return {};
    }

    switch (section) {
    case ColumnIsbn:
        return QStringLiteral("ISBN");
    case ColumnName:
        return QStringLiteral("书名");
    case ColumnAuthor:
        return QStringLiteral("作者");
    case ColumnTotal:
        return QStringLiteral("馆藏总量");
    case ColumnAvailable:
        return QStringLiteral("可借数量");
    case ColumnBorrowed:
        return QStringLiteral("已借出");
    default:
        return {};
    }
}

} // namespace libms
