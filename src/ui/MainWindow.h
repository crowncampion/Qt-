#pragma once

#include "core/AppConfig.h"
#include "ui/PageId.h"

#include <QMainWindow>

#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

namespace libms {

class BookRepository;
class Database;
class HomePage;
class LibraryService;
class LoanRepository;
class OverviewPage;
class Page;

/**
 * @brief 应用主窗口。
 *
 * 承担三件事：
 *  1. 组装对象图（Database -> Repository -> LibraryService -> Page）；
 *  2. 提供页面栈与导航（按 PageId 具名寻址，不再用裸索引）；
 *  3. 承载状态栏，统一显示各页反馈的信息。
 *
 * 所有依赖都以 std::unique_ptr 持有，析构顺序由成员声明顺序决定。
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    /**
     * @brief 建立数据库连接。
     * @return 失败时返回错误信息，供调用方决定是否终止启动。
     */
    QString initialize();

private slots:
    /// 切换到指定页面。
    void navigateTo(PageId pageId);

    /// 返回主菜单。
    void goHome();

    /// 在状态栏显示一条信息。
    void showStatusMessage(const QString &message);

private:
    /// 创建全部功能页并加入页面栈。
    void buildPages();

    /// 把某个页面注册进 QStackedWidget 与页面索引表。
    void registerPage(PageId pageId, Page *page);

    /// 首次进入总览页时才创建它，缩短启动时间。
    Page *overviewPage();

    /// 同步窗口标题与页面栈当前页。
    void updateWindowTitle();

    Ui::MainWindow *ui = nullptr;

    std::unique_ptr<Database> m_database;
    std::unique_ptr<BookRepository> m_bookRepository;
    std::unique_ptr<LoanRepository> m_loanRepository;
    std::unique_ptr<LibraryService> m_service;

    HomePage *m_homePage = nullptr;
    OverviewPage *m_overviewPage = nullptr;
    /// PageId -> 页面栈索引。
    QHash<int, int> m_pageIndices;
};

} // namespace libms
