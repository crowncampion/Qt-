#include "ui/pages/ReturnBookPage.h"

#include "core/Book.h"
#include "service/LibraryService.h"
#include "ui/dialogs/ReturnBookDialog.h"
#include "ui_ReturnBookPage.h"

#include <QHeaderView>
#include <QPushButton>

namespace libms {

ReturnBookPage::ReturnBookPage(LibraryService &service, QWidget *parent)
    : Page(service, parent)
    , ui(new Ui::ReturnBookPage)
    , m_model(new LoanRecordTableModel(this))
{
    ui->setupUi(this);

    setupHeader(ui->titleLabel, ui->subtitleLabel, ui->backButton);

    ui->loansTableView->setModel(m_model);
    ui->loansTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->loansTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->loansTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->loansTableView->setAlternatingRowColors(true);
    ui->loansTableView->verticalHeader()->setVisible(false);
    ui->loansTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->loansTableView->horizontalHeader()->setSectionResizeMode(LoanRecordTableModel::ColumnIsbn,
                                                                 QHeaderView::Stretch);

    connect(ui->returnButton, &QPushButton::clicked, this, &ReturnBookPage::returnSelected);
    ui->returnButton->setProperty("primary", true);
    connect(ui->refreshButton, &QPushButton::clicked, this, &ReturnBookPage::refresh);
    connect(ui->loansTableView, &QAbstractItemView::doubleClicked, this,
            &ReturnBookPage::handleDoubleClick);
    connect(ui->loansTableView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &ReturnBookPage::updateActionState);

    refresh();
}

ReturnBookPage::~ReturnBookPage()
{
    delete ui;
}

void ReturnBookPage::refresh()
{
    m_model->setRecords(service().outstandingLoans());
    updateActionState();
}

void ReturnBookPage::updateActionState()
{
    const bool hasSelection = ui->loansTableView->currentIndex().isValid();
    ui->returnButton->setEnabled(hasSelection);

    if (m_model->count() == 0) {
        ui->emptyHintLabel->setText(QStringLiteral("当前没有未归还的借阅记录。"));
    } else {
        ui->emptyHintLabel->setText(QStringLiteral("共 %1 条未归还记录；双击某一行即可办理归还。")
                                        .arg(m_model->count()));
    }
}

void ReturnBookPage::handleDoubleClick(const QModelIndex &index)
{
    if (index.isValid()) {
        returnSelected();
    }
}

void ReturnBookPage::returnSelected()
{
    const QModelIndex current = ui->loansTableView->currentIndex();
    if (!current.isValid()) {
        notifyError(QStringLiteral("请先在列表中选择一条借阅记录"), QStringLiteral("无法归还"));
        return;
    }

    const LendRecord record = m_model->recordAt(current.row());
    if (!record.isValid()) {
        notifyError(QStringLiteral("未能读取该借阅记录，请刷新后重试"), QStringLiteral("无法归还"));
        return;
    }

    // 书名仅用于提示，图书可能已被删除。
    const Result<Book> book = service().findBookByIsbn(record.isbn);
    const QString bookName = book ? book.value().name : QString();

    ReturnBookDialog dialog(record, bookName, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const int count = dialog.returnCount();
    const VoidResult result = service().returnBook(record.id, count);
    if (!result) {
        notifyError(result.error(), QStringLiteral("无法归还"));
        return;
    }

    notify(QStringLiteral("已登记归还：%1 归还 %2 本").arg(record.readerId).arg(count));
    refresh();
}

} // namespace libms
