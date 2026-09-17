#pragma once

#include "ui/PageId.h"
#include "ui/pages/Page.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class HomePage;
}
QT_END_NAMESPACE

namespace libms {

/**
 * @brief 主菜单页。
 *
 * 只负责发出导航请求，不直接操作 QStackedWidget（原实现把界面控制权散落在各处的 lambda 里）。
 */
class HomePage : public Page
{
    Q_OBJECT

public:
    explicit HomePage(LibraryService &service, QWidget *parent = nullptr);
    ~HomePage() override;

    /// 刷新统计信息（在馆图书种类数与未归还记录数）。
    void refresh() override;

signals:
    /// 请求跳转到指定功能页。
    void navigateTo(PageId pageId);

private:
    Ui::HomePage *ui = nullptr;
};

} // namespace libms
