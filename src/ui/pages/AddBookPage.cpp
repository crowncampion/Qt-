#include "ui/pages/AddBookPage.h"

#include "service/LibraryService.h"
#include "ui_AddBookPage.h"

#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

namespace libms {

AddBookPage::AddBookPage(LibraryService &service, QWidget *parent)
    : Page(service, parent)
    , ui(new Ui::AddBookPage)
{
    ui->setupUi(this);

    setupHeader(ui->titleLabel, ui->subtitleLabel, ui->backButton);

    // ISBN 只允许 0~13 位数字：既防止输入汉字，也避免超长。
    ui->isbnEdit->setValidator(
        new QRegularExpressionValidator(QRegularExpression(QStringLiteral("^\\d{0,13}$")), this));
    ui->isbnEdit->setPlaceholderText(QStringLiteral("13 位数字，例如 9787115428028"));

    connect(ui->confirmButton, &QPushButton::clicked, this, &AddBookPage::submit);
    connect(ui->resetButton, &QPushButton::clicked, this, &AddBookPage::clearForm);

    // 主操作按钮使用主色实心样式。
    ui->confirmButton->setProperty("primary", true);

    // 在任意输入框按回车即提交，符合表单填写习惯。
    connect(ui->isbnEdit, &QLineEdit::returnPressed, this, &AddBookPage::submit);
    connect(ui->nameEdit, &QLineEdit::returnPressed, this, &AddBookPage::submit);
    connect(ui->authorEdit, &QLineEdit::returnPressed, this, &AddBookPage::submit);

    clearForm();
}

AddBookPage::~AddBookPage()
{
    delete ui;
}

void AddBookPage::refresh()
{
    clearForm();
}

void AddBookPage::clearForm()
{
    ui->isbnEdit->clear();
    ui->nameEdit->clear();
    ui->authorEdit->clear();
    ui->totalSpinBox->setValue(1);
    ui->isbnEdit->setFocus();
}

void AddBookPage::submit()
{
    const QString isbn = ui->isbnEdit->text().trimmed();
    const QString name = ui->nameEdit->text();
    const QString author = ui->authorEdit->text();
    const int total = ui->totalSpinBox->value();

    const VoidResult result = service().addBook(name, author, isbn, total);
    if (!result) {
        notifyError(result.error(), QStringLiteral("无法录入"));
        return;
    }

    QMessageBox::information(this,
                             successTitle(),
                             QStringLiteral("《%1》已成功录入，馆藏 %2 本。").arg(name.trimmed()).arg(total));
    clearForm();
    notify(QStringLiteral("已录入新书《%1》").arg(name.trimmed()));
}

} // namespace libms
