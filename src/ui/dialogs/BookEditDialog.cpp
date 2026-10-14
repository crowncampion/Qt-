#include "ui/dialogs/BookEditDialog.h"

#include "service/LibraryService.h"
#include "ui_BookEditDialog.h"

#include <QMessageBox>
#include <QPushButton>

namespace libms {

BookEditDialog::BookEditDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::BookEditDialog)
{
    ui->setupUi(this);

    // 拦截「确定」按钮：先做本地校验，通过后才真正 accept()。
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &BookEditDialog::validateAndAccept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

BookEditDialog::~BookEditDialog()
{
    delete ui;
}

void BookEditDialog::prepareForEdit(const QString &isbn, const BookInput &input)
{
    setWindowTitle(QStringLiteral("修改图书信息"));
    ui->isbnEdit->setText(isbn);
    ui->nameEdit->setText(input.name);
    ui->authorEdit->setText(input.author);
    ui->totalSpinBox->setValue(input.total);

    // ISBN 是图书的业务主键，修改模式下不允许改动。
    ui->isbnEdit->setReadOnly(true);
    ui->isbnEdit->setToolTip(QStringLiteral("ISBN 不可修改"));
    ui->nameEdit->setFocus();
    ui->nameEdit->selectAll();
}

void BookEditDialog::prepareForCreate()
{
    setWindowTitle(QStringLiteral("新增图书"));
    ui->isbnEdit->clear();
    ui->nameEdit->clear();
    ui->authorEdit->clear();
    ui->totalSpinBox->setValue(1);

    ui->isbnEdit->setReadOnly(false);
    ui->isbnEdit->setToolTip(LibraryService::isbnRuleText());
    ui->nameEdit->setFocus();
}

BookEditDialog::BookInput BookEditDialog::input() const
{
    BookInput result;
    result.name = ui->nameEdit->text().trimmed();
    result.author = ui->authorEdit->text().trimmed();
    result.total = ui->totalSpinBox->value();
    return result;
}

void BookEditDialog::validateAndAccept()
{
    const BookInput current = input();

    // ISBN 只在新增模式下由用户填写，因此仅在可编辑时校验。
    if (!ui->isbnEdit->isReadOnly()) {
        const QString isbn = ui->isbnEdit->text().trimmed();
        if (!LibraryService::isValidIsbn(isbn)) {
            QMessageBox::warning(this, QStringLiteral("输入有误"), LibraryService::isbnRuleText());
            ui->isbnEdit->setFocus();
            return;
        }
    }

    if (current.name.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("输入有误"), QStringLiteral("书名不能为空"));
        ui->nameEdit->setFocus();
        return;
    }
    if (current.author.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("输入有误"), QStringLiteral("作者不能为空"));
        ui->authorEdit->setFocus();
        return;
    }

    accept();
}

} // namespace libms
