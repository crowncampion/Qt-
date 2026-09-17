#pragma once

#include "core/SearchMode.h"
#include "ui/models/BookTableModel.h"
#include "ui/pages/Page.h"

#include <QRegularExpressionValidator>

QT_BEGIN_NAMESPACE
namespace Ui {
class ManageBooksPage;
}
QT_END_NAMESPACE

namespace libms {

/**
 * @brief 管理已有图书页：检索、修改与删除。
 *
 * 检索方式改为显式下拉选择（原实现用滚轮在两个输入框之间切换，用户无法察觉），
 * 并补齐了「修改」能力——原实现只能双击弹出对话框，没有任何可见提示。
 */
class ManageBooksPage : public Page
{
    Q_OBJECT

public:
    explicit ManageBooksPage(LibraryService &service, QWidget *parent = nullptr);
    ~ManageBooksPage() override;

    /// 重新载入全部图书。
    void refresh() override;

private slots:
    /// 按当前检索方式查询。
    void search();

    /// 载入全部图书并清空检索条件。
    void showAll();

    /// 删除当前选中的图书。
    void removeSelected();

    /// 修改当前选中的图书。
    void editSelected();

    /// 双击表格行进入修改。
    void handleDoubleClick(const QModelIndex &index);

    /// 检索方式变化时切换输入框校验规则。
    void handleSearchModeChanged(int index);

    /// 选中行变化时更新按钮可用状态。
    void updateActionState();

private:
    /// 当前选中的图书；未选中时返回缺省对象（id 为 0）。
    Book selectedBook() const;

    /// 当前检索方式。
    SearchMode currentSearchMode() const;

    Ui::ManageBooksPage *ui = nullptr;
    BookTableModel *m_model = nullptr;
    QRegularExpressionValidator *m_isbnValidator = nullptr;
};

} // namespace libms
