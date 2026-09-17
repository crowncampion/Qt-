#pragma once

#include "ui/pages/Page.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class AddBookPage;
}
QT_END_NAMESPACE

namespace libms {

/**
 * @brief 添加图书页。
 *
 * 输入校验交给 LibraryService::addBook，本页只负责收集输入与呈现结果。
 */
class AddBookPage : public Page
{
    Q_OBJECT

public:
    explicit AddBookPage(LibraryService &service, QWidget *parent = nullptr);
    ~AddBookPage() override;

    /// 进入页面时清空上次残留的输入。
    void refresh() override;

private slots:
    /// 校验并提交新书。
    void submit();

    /// 清空表单并聚焦到 ISBN 输入框。
    void clearForm();

private:
    Ui::AddBookPage *ui = nullptr;
};

} // namespace libms
