#include "ui/pages/HomePage.h"

#include "service/LibraryService.h"
#include "ui_HomePage.h"

#include <QPushButton>
#include <QVector>

namespace libms {
namespace {

/// 主菜单按钮与目标页面的对应关系。
struct NavigationEntry
{
    QPushButton *button;
    PageId pageId;
};

} // namespace

HomePage::HomePage(LibraryService &service, QWidget *parent)
    : Page(service, parent)
    , ui(new Ui::HomePage)
{
    ui->setupUi(this);

    // 主菜单是根页面，没有返回按钮，因此 backButton 传 nullptr。
    setupHeader(ui->titleLabel, ui->subtitleLabel, /*backButton=*/nullptr);

    const QVector<NavigationEntry> entries = {
        {ui->addBookButton, PageId::AddBook},
        {ui->manageBooksButton, PageId::ManageBooks},
        {ui->lendBookButton, PageId::LendBook},
        {ui->returnBookButton, PageId::ReturnBook},
        {ui->overviewButton, PageId::Overview},
    };

    for (const NavigationEntry &entry : entries) {
        entry.button->setCursor(Qt::PointingHandCursor);
        // 供样式表 QPushButton[navButton="true"] 命中。
        entry.button->setProperty("navButton", true);
        const PageId target = entry.pageId;
        connect(entry.button, &QPushButton::clicked, this, [this, target] { emit navigateTo(target); });
    }
}

HomePage::~HomePage()
{
    delete ui;
}

void HomePage::refresh()
{
    const int titleCount = service().allBooks().size();
    const int outstandingCount = service().outstandingLoans().size();

    ui->statisticsLabel->setText(QStringLiteral("当前在馆图书 %1 种 ． 未归还借阅记录 %2 条")
                                     .arg(titleCount)
                                     .arg(outstandingCount));
}

} // namespace libms
