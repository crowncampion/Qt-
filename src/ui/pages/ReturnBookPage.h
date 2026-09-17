#pragma once

#include "ui/models/LoanRecordTableModel.h"
#include "ui/pages/Page.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class ReturnBookPage;
}
QT_END_NAMESPACE

namespace libms {

/**
 * @brief 归还图书页。
 *
 * 只列出「尚有未归还数量」的借阅记录，避免用户在已还完的记录上反复操作。
 */
class ReturnBookPage : public Page
{
    Q_OBJECT

public:
    explicit ReturnBookPage(LibraryService &service, QWidget *parent = nullptr);
    ~ReturnBookPage() override;

    /// 重新载入未归还记录。
    void refresh() override;

private slots:
    /// 为选中的记录办理归还。
    void returnSelected();

    /// 双击表格行直接办理归还。
    void handleDoubleClick(const QModelIndex &index);

    /// 选中行变化时更新按钮可用状态。
    void updateActionState();

private:
    Ui::ReturnBookPage *ui = nullptr;
    LoanRecordTableModel *m_model = nullptr;
};

} // namespace libms
