#include "ui/MainWindow.h"

#include "data/BookRepository.h"
#include "data/Database.h"
#include "data/LoanRepository.h"
#include "service/LibraryService.h"
#include "ui/pages/AddBookPage.h"
#include "ui/pages/HomePage.h"
#include "ui/pages/LendBookPage.h"
#include "ui/pages/ManageBooksPage.h"
#include "ui/pages/OverviewPage.h"
#include "ui/pages/Page.h"
#include "ui/pages/ReturnBookPage.h"
#include "ui_MainWindow.h"

#include <QFile>
#include <QHash>
#include <QMessageBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>

namespace libms {
namespace {

/// 状态栏提示的显示时长。
constexpr int kStatusMessageTimeoutMs = 6000;

/// 各页面对应的窗口标题。
QString windowTitleFor(PageId pageId)
{
    switch (pageId) {
    case PageId::Home:
        return QStringLiteral("图书馆借阅管理系统");
    case PageId::AddBook:
        return QStringLiteral("图书馆借阅管理系统 － 添加图书");
    case PageId::ManageBooks:
        return QStringLiteral("图书馆借阅管理系统 － 管理已有图书");
    case PageId::LendBook:
        return QStringLiteral("图书馆借阅管理系统 － 借阅图书");
    case PageId::ReturnBook:
        return QStringLiteral("图书馆借阅管理系统 － 归还图书");
    case PageId::Overview:
        return QStringLiteral("图书馆借阅管理系统 － 出借与归还总览");
    }
    return QStringLiteral("图书馆借阅管理系统");
}

/// 读取内置的样式表；缺失时不阻塞启动，只记录日志。
QString loadStyleSheet()
{
    QFile file(QStringLiteral(":/styles/app.qss"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("未能加载样式表 :/styles/app.qss");
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
    // 其余成员为 unique_ptr，按逆序自动析构：
    // LibraryService -> Repository -> Database（Database 析构时关闭连接）。
}

QString MainWindow::initialize()
{
    const DatabaseConfig config = AppConfig::load();

    m_database = std::make_unique<Database>();
    const VoidResult opened = m_database->open(config);
    if (!opened) {
        return opened.error();
    }

    m_bookRepository = std::make_unique<BookRepository>(*m_database);
    m_loanRepository = std::make_unique<LoanRepository>(*m_database);
    m_service = std::make_unique<LibraryService>(*m_bookRepository, *m_loanRepository);

    // 数据库就绪后再构建界面：连接失败时不必初始化任何页面。
    setStyleSheet(loadStyleSheet());
    buildPages();

    statusBar()->showMessage(
        QStringLiteral("已连接到数据库 %1@%2:%3").arg(config.databaseName, config.hostName).arg(config.port),
        kStatusMessageTimeoutMs);

    navigateTo(PageId::Home);
    return {};
}

void MainWindow::buildPages()
{
    // 页面栈由代码装配，各页在 Designer 中独立维护自己的 .ui 文件。
    ui->stackedWidget->setCurrentIndex(0);

    m_homePage = new HomePage(*m_service, this);
    connect(m_homePage, &HomePage::navigateTo, this, &MainWindow::navigateTo);
    registerPage(PageId::Home, m_homePage);

    registerPage(PageId::AddBook, new AddBookPage(*m_service, this));
    registerPage(PageId::ManageBooks, new ManageBooksPage(*m_service, this));
    registerPage(PageId::LendBook, new LendBookPage(*m_service, this));
    registerPage(PageId::ReturnBook, new ReturnBookPage(*m_service, this));
    // 总览页数据量较大，等到第一次进入时再创建。
}

void MainWindow::registerPage(PageId pageId, Page *page)
{
    const int index = ui->stackedWidget->addWidget(page);
    m_pageIndices.insert(static_cast<int>(pageId), index);

    connect(page, &Page::backRequested, this, &MainWindow::goHome);
    connect(page, &Page::statusMessage, this, &MainWindow::showStatusMessage);
}

Page *MainWindow::overviewPage()
{
    if (m_overviewPage == nullptr) {
        m_overviewPage = new OverviewPage(*m_service, this);
        registerPage(PageId::Overview, m_overviewPage);
    }
    return m_overviewPage;
}

void MainWindow::navigateTo(PageId pageId)
{
    if (pageId == PageId::Overview) {
        overviewPage();
    }

    const int index = m_pageIndices.value(static_cast<int>(pageId), -1);
    if (index < 0) {
        qWarning("请求的页面未注册: %d", static_cast<int>(pageId));
        return;
    }

    ui->stackedWidget->setCurrentIndex(index);

    // 进入页面时刷新数据，保证各页看到的是最新状态。
    if (auto *page = qobject_cast<Page *>(ui->stackedWidget->widget(index))) {
        page->refresh();
    }

    updateWindowTitle();
}

void MainWindow::goHome()
{
    navigateTo(PageId::Home);
}

void MainWindow::updateWindowTitle()
{
    const int index = ui->stackedWidget->currentIndex();
    for (auto it = m_pageIndices.constBegin(); it != m_pageIndices.constEnd(); ++it) {
        if (it.value() == index) {
            setWindowTitle(windowTitleFor(static_cast<PageId>(it.key())));
            return;
        }
    }
    setWindowTitle(windowTitleFor(PageId::Home));
}

void MainWindow::showStatusMessage(const QString &message)
{
    statusBar()->showMessage(message, kStatusMessageTimeoutMs);
}

} // namespace libms
