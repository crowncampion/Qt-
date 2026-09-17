#pragma once

#include "ui/models/LoanRecordTableModel.h"
#include "ui/models/ReturnRecordTableModel.h"
#include "ui/pages/Page.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class OverviewPage;
}
QT_END_NAMESPACE

namespace libms {

/**
 * @brief 出借与归还总览页。
 *
 * 原实现的这一页存在导航错位（「出借总览」实际打开了归还页），并且两张表被并排挤在同一行；
 * 现在改为标签页展示，并补充总计信息。
 */
class OverviewPage : public Page
{
    Q_OBJECT

public:
    explicit OverviewPage(LibraryService &service, QWidget *parent = nullptr);
    ~OverviewPage() override;

    /// 重新载入借阅与归还记录。
    void refresh() override;

private slots:
    /// 在标签页切换时同步状态信息。
    void handleTabChanged(int index);

private:
    /// 刷新顶部统计信息。
    void updateSummary();

    Ui::OverviewPage *ui = nullptr;
    LoanRecordTableModel *m_loanModel = nullptr;
    ReturnRecordTableModel *m_returnModel = nullptr;
};

} // namespace libms
