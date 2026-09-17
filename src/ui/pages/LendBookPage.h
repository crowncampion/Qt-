#pragma once

#include "ui/models/BookTableModel.h"
#include "ui/pages/Page.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class LendBookPage;
}
QT_END_NAMESPACE

namespace libms {

/**
 * @brief 借阅图书页。
 *
 * 左侧登记借阅信息，右侧展示可借图书便于核对 ISBN。
 * 借出会同时写入借阅记录并扣减可借数量，由 LibraryService 在一个事务中完成。
 */
class LendBookPage : public Page
{
    Q_OBJECT

public:
    explicit LendBookPage(LibraryService &service, QWidget *parent = nullptr);
    ~LendBookPage() override;

    /// 刷新可借图书列表。
    void refresh() override;

private slots:
    /// 提交借阅。
    void submit();

    /// 双击可借图书列表时把该书的 ISBN 填入输入框。
    void fillIsbnFromTable(const QModelIndex &index);

private:
    Ui::LendBookPage *ui = nullptr;
    BookTableModel *m_model = nullptr;
};

} // namespace libms
