#include "ui/pages/LendBookPage.h"

#include "service/LibraryService.h"
#include "ui_LendBookPage.h"

#include <QHeaderView>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

namespace libms {

LendBookPage::LendBookPage(LibraryService &service, QWidget *parent)
    : Page(service, parent)
    , ui(new Ui::LendBookPage)
    , m_model(new BookTableModel(this))
{
    ui->setupUi(this);

    setupHeader(ui->titleLabel, ui->subtitleLabel, ui->backButton);

    ui->availableTableView->setModel(m_model);
    ui->availableTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->availableTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->availableTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->availableTableView->setAlternatingRowColors(true);
    ui->availableTableView->verticalHeader()->setVisible(false);
    ui->availableTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->availableTableView->horizontalHeader()->setSectionResizeMode(BookTableModel::ColumnName,
                                                                    QHeaderView::Stretch);

    // ISBN 与借阅人姓名都做输入期校验，避免提交后才报错。
    ui->isbnEdit->setValidator(
        new QRegularExpressionValidator(QRegularExpression(QStringLiteral("^\\d{0,13}$")), this));
    ui->isbnEdit->setPlaceholderText(QStringLiteral("13 位数字"));
    ui->readerEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^[\\x{4e00}-\\x{9fa5}A-Za-z]{0,20}$")), this));
    ui->readerEdit->setPlaceholderText(QStringLiteral("2~20 个汉字或英文字母"));

    connect(ui->confirmButton, &QPushButton::clicked, this, &LendBookPage::submit);
    ui->confirmButton->setProperty("primary", true);
    connect(ui->isbnEdit, &QLineEdit::returnPressed, this, &LendBookPage::submit);
    connect(ui->readerEdit, &QLineEdit::returnPressed, this, &LendBookPage::submit);
    connect(ui->availableTableView, &QAbstractItemView::doubleClicked, this,
            &LendBookPage::fillIsbnFromTable);

    refresh();
}

LendBookPage::~LendBookPage()
{
    delete ui;
}

void LendBookPage::refresh()
{
    m_model->setBooks(service().allBooks());
}

void LendBookPage::fillIsbnFromTable(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }
    ui->isbnEdit->setText(m_model->bookAt(index.row()).isbn);
    ui->readerEdit->setFocus();
}

void LendBookPage::submit()
{
    const QString isbn = ui->isbnEdit->text().trimmed();
    const QString reader = ui->readerEdit->text().trimmed();
    const int count = ui->countSpinBox->value();

    const VoidResult result = service().lendBook(isbn, reader, count);
    if (!result) {
        notifyError(result.error(), QStringLiteral("无法借出"));
        return;
    }

    QMessageBox::information(
        this,
        successTitle(),
        QStringLiteral("已为「%1」办理借阅：ISBN %2，共 %3 本。").arg(reader, isbn).arg(count));

    ui->isbnEdit->clear();
    ui->readerEdit->clear();
    ui->countSpinBox->setValue(1);
    ui->isbnEdit->setFocus();

    refresh();
    notify(QStringLiteral("已办理借阅：%1 借出 %2 本").arg(reader).arg(count));
}

} // namespace libms
