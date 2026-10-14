#pragma once

#include "core/LendRecord.h"

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui {
class ReturnBookDialog;
}
QT_END_NAMESPACE

namespace libms {

/**
 * @brief 归还图书的确认对话框。
 *
 * 只展示待归还记录并收集归还数量；写库由 LibraryService::returnBook 在事务中完成，
 * 原实现是在对话框内部直接执行 INSERT/UPDATE，职责错位且缺少校验。
 */
class ReturnBookDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief 构造对话框。
     * @param record 待归还的借阅记录。
     * @param bookName 图书书名，用于提示；为空时显示 ISBN。
     * @param parent 父窗口。
     */
    explicit ReturnBookDialog(const LendRecord &record, const QString &bookName, QWidget *parent = nullptr);
    ~ReturnBookDialog() override;

    /// 用户选择的归还数量；对话框被取消时返回 0。
    int returnCount() const;

private:
    Ui::ReturnBookDialog *ui = nullptr;
    int m_returnCount = 0;
};

} // namespace libms
