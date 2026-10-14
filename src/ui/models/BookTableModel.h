#pragma once

#include "core/Book.h"

#include <QAbstractTableModel>
#include <QVector>

namespace libms {

/**
 * @brief 图书列表的表格模型。
 *
 * 数据来自 BookRepository，以值语义保存在内存中；
 * 相比 QSqlQueryModel 的好处是可以直接取回强类型对象，且不依赖数据库连接的存活。
 */
class BookTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        ColumnIsbn = 0,
        ColumnName,
        ColumnAuthor,
        ColumnTotal,
        ColumnAvailable,
        ColumnBorrowed,
        ColumnCount,
    };

    explicit BookTableModel(QObject *parent = nullptr);

    /// 用新的图书列表整体替换现有内容。
    void setBooks(const QVector<Book> &books);

    /// 取第 @p row 行的图书；越界时返回缺省构造的对象。
    Book bookAt(int row) const;

    /// 当前行数。
    int count() const { return m_books.size(); }

private:
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    QVector<Book> m_books;
};

} // namespace libms
