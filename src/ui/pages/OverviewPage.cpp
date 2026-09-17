#include "ui/pages/OverviewPage.h"

#include "service/LibraryService.h"
#include "ui_OverviewPage.h"

#include <QHeaderView>
#include <QPushButton>

namespace libms {
namespace {

/// 让表格列宽自适应内容，并让最宽的一列占据剩余空间。
void configureTable(QTableView *tableView, int stretchColumn)
{
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableView->setAlternatingRowColors(true);
    tableView->verticalHeader()->setVisible(false);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->setSectionResizeMode(stretchColumn, QHeaderView::Stretch);
}

} // namespace

OverviewPage::OverviewPage(LibraryService &service, QWidget *parent)
    : Page(service, parent)
    , ui(new Ui::OverviewPage)
    , m_loanModel(new LoanRecordTableModel(this))
    , m_returnModel(new ReturnRecordTableModel(this))
{
    ui->setupUi(this);

    setupHeader(ui->titleLabel, ui->subtitleLabel, ui->backButton);

    ui->loansTableView->setModel(m_loanModel);
    configureTable(ui->loansTableView, LoanRecordTableModel::ColumnIsbn);

    ui->returnsTableView->setModel(m_returnModel);
    configureTable(ui->returnsTableView, ReturnRecordTableModel::ColumnIsbn);

    connect(ui->refreshButton, &QPushButton::clicked, this, &OverviewPage::refresh);
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &OverviewPage::handleTabChanged);

    refresh();
}

OverviewPage::~OverviewPage()
{
    delete ui;
}

void OverviewPage::refresh()
{
    const QVector<LendRecord> loans = service().allLoans();
    const QVector<ReturnRecord> returns = service().allReturns();

    m_loanModel->setRecords(loans);
    // 归还记录需要借阅记录补充上下文，因此把同一份借阅数据传入。
    m_returnModel->setRecords(returns, loans);

    updateSummary();
    handleTabChanged(ui->tabWidget->currentIndex());
}

void OverviewPage::updateSummary()
{
    int lentCopies = 0;
    int outstandingCopies = 0;
    for (int row = 0; row < m_loanModel->count(); ++row) {
        const LendRecord record = m_loanModel->recordAt(row);
        lentCopies += record.borrowCount;
        outstandingCopies += record.outstandingCount();
    }

    ui->summaryLabel->setText(QStringLiteral("借阅记录 %1 条（累计借出 %2 本）． 归还记录 %3 条 ． 未归还 %4 本")
                                  .arg(m_loanModel->count())
                                  .arg(lentCopies)
                                  .arg(m_returnModel->count())
                                  .arg(outstandingCopies));
}

void OverviewPage::handleTabChanged(int index)
{
    ui->tabHintLabel->setText(index == 0
                                  ? QStringLiteral("全部借阅记录，按借出时间倒序排列。")
                                  : QStringLiteral("全部归还流水，按归还时间倒序排列。"));
}

} // namespace libms
