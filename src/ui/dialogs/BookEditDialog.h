#pragma once

#include <QDialog>
#include <QString>

QT_BEGIN_NAMESPACE
namespace Ui {
class BookEditDialog;
}
QT_END_NAMESPACE

namespace libms {

/**
 * @brief 新增 / 修改图书信息的对话框。
 *
 * 只负责收集与回显数据，不接触数据库（原实现由 Dialog 直接下发 UPDATE 语句）。
 */
class BookEditDialog : public QDialog
{
    Q_OBJECT

public:
    /// 对话框中编辑的字段集合。
    struct BookInput
    {
        QString name;
        QString author;
        int total = 0;
    };

    /**
     * @brief 构造对话框。
     * @param parent 父窗口。
     */
    explicit BookEditDialog(QWidget *parent = nullptr);
    ~BookEditDialog() override;

    /**
     * @brief 以修改模式打开：预填现有数据且 ISBN 只读。
     * @param isbn 不可修改的 ISBN。
     * @param input 现有图书信息。
     */
    void prepareForEdit(const QString &isbn, const BookInput &input);

    /**
     * @brief 以新增模式打开：清空输入并聚焦书名。
     */
    void prepareForCreate();

    /// 读取当前输入值。
    BookInput input() const;

private slots:
    /// 校验输入，不合法时提示并阻止对话框关闭。
    void validateAndAccept();

private:
    Ui::BookEditDialog *ui = nullptr;
};

} // namespace libms
