#include "ui/MainWindow.h"

#include "ui/PageId.h"

#include <QApplication>
#include <QDebug>
#include <QList>
#include <QPushButton>
#include <QStackedWidget>
#include <QString>
#include <QStringList>
#include <QTableView>
#include <QTimer>
#include <QVector>

#include <cstdio>

namespace {

/// 报告文件句柄；用纯 C 输出，避免依赖 Qt 的流对象在无控制台环境下的行为。
FILE *g_report = nullptr;

void report(const QString &line)
{
    if (g_report != nullptr) {
        std::fprintf(g_report, "%s\n", line.toUtf8().constData());
        std::fflush(g_report);
    }
    std::fprintf(stderr, "%s\n", line.toUtf8().constData());
    std::fflush(stderr);
}

int g_failures = 0;

void check(bool condition, const QString &what)
{
    report(QStringLiteral("  [%1] %2").arg(condition ? QStringLiteral("PASS") : QStringLiteral("FAIL"), what));
    if (!condition) {
        ++g_failures;
    }
}

/// 按标题前缀查找按钮（主菜单按钮的标题是「标题\n说明」两行）。
QPushButton *buttonStartingWith(QWidget *root, const QString &prefix)
{
    const QList<QPushButton *> buttons = root->findChildren<QPushButton *>();
    for (QPushButton *button : buttons) {
        if (button->text().startsWith(prefix)) {
            return button;
        }
    }
    return nullptr;
}

/// 收集 Qt 日志，用于断言运行期间没有意外告警。
QStringList g_messages;

void messageHandler(QtMsgType type, const QMessageLogContext &, const QString &message)
{
    if (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg) {
        g_messages.append(message);
    }
}

} // namespace

/**
 * @brief 界面冒烟测试。
 *
 * 真实构造整个界面，逐页进入并校验关键控件与数据模型，
 * 覆盖历史上出过问题的「导航错位」与「未选中行即操作」两类缺陷。
 */
int main(int argc, char *argv[])
{
    g_report = std::fopen("gui-smoke-results.txt", "w");

    qInstallMessageHandler(messageHandler);

    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("图书馆借阅管理系统"));

    report(QStringLiteral("===== GUI 冒烟测试 ====="));

    libms::MainWindow window;
    const QString initError = window.initialize();
    if (!initError.isEmpty()) {
        report(QStringLiteral("  [SKIP] 无法连接数据库，跳过界面测试: %1").arg(initError));
        if (g_report != nullptr) {
            std::fclose(g_report);
        }
        return 0;
    }

    check(true, QStringLiteral("主窗口初始化成功"));

    auto *stack = window.findChild<QStackedWidget *>(QStringLiteral("stackedWidget"));
    check(stack != nullptr, QStringLiteral("找到页面栈 stackedWidget"));
    if (stack == nullptr) {
        if (g_report != nullptr) {
            std::fclose(g_report);
        }
        return 1;
    }

    check(stack->count() == 5,
          QStringLiteral("启动时注册 5 个页面（总览页延迟创建），实际 %1").arg(stack->count()));

    // 逐页进入并校验该页独有的控件，可发现页面与索引错位的问题。
    struct PageExpectation
    {
        const char *buttonPrefix;
        const char *expectedWidget;
        const char *description;
    };

    const QVector<PageExpectation> expectations = {
        {"添加图书", "isbnEdit", "添加图书页"},
        {"管理已有图书", "searchModeComboBox", "管理图书页"},
        {"借阅图书", "availableTableView", "借阅图书页"},
        {"归还图书", "loansTableView", "归还图书页"},
        {"出借与归还总览", "tabWidget", "总览页"},
    };

    for (const PageExpectation &expectation : expectations) {
        const QString prefix = QString::fromUtf8(expectation.buttonPrefix);
        const QString widgetName = QString::fromUtf8(expectation.expectedWidget);
        const QString description = QString::fromUtf8(expectation.description);

        QPushButton *button = buttonStartingWith(&window, prefix);
        check(button != nullptr, QStringLiteral("主菜单存在「%1」入口").arg(prefix));
        if (button == nullptr) {
            continue;
        }

        button->click();
        QApplication::processEvents();

        QWidget *page = stack->currentWidget();
        const bool onExpectedPage =
            page != nullptr && page->findChild<QWidget *>(widgetName) != nullptr;
        check(onExpectedPage,
              QStringLiteral("点击「%1」后进入%2（当前页应含控件 %3）")
                  .arg(prefix, description, widgetName));

        if (page != nullptr && widgetName == QStringLiteral("loansTableView")) {
            if (auto *returnButton = page->findChild<QPushButton *>(QStringLiteral("returnButton"))) {
                check(!returnButton->isEnabled(),
                      QStringLiteral("未选择记录时「办理归还」按钮为禁用状态"));
            }
            if (auto *table = page->findChild<QTableView *>(QStringLiteral("loansTableView"))) {
                check(table->model() != nullptr && table->model()->columnCount() > 0,
                      QStringLiteral("归还页表格模型已装载"));
            }
        }

        if (page != nullptr && widgetName == QStringLiteral("searchModeComboBox")) {
            if (auto *table = page->findChild<QTableView *>(QStringLiteral("tableView"))) {
                const int rows = table->model() != nullptr ? table->model()->rowCount() : -1;
                check(rows > 0, QStringLiteral("管理页默认列出全部图书（%1 行）").arg(rows));
            }
            if (auto *deleteButton = page->findChild<QPushButton *>(QStringLiteral("deleteButton"))) {
                check(!deleteButton->isEnabled(),
                      QStringLiteral("未选择图书时「删除选中图书」按钮为禁用状态"));
            }
        }

        // 返回主菜单，为下一轮做准备。
        if (auto *backButton = page->findChild<QPushButton *>(QStringLiteral("backButton"))) {
            backButton->click();
            QApplication::processEvents();
        }
        check(stack->currentIndex() == 0,
              QStringLiteral("「%1」页可返回主菜单，当前索引 %2").arg(prefix).arg(stack->currentIndex()));
    }

    // 总览页：窗口标题必须体现「总览」，而不是历史上错位的「归还」。
    if (QPushButton *overviewButton = buttonStartingWith(&window, QStringLiteral("出借与归还总览"))) {
        overviewButton->click();
        QApplication::processEvents();
        check(window.windowTitle().contains(QStringLiteral("总览")),
              QStringLiteral("总览页窗口标题正确: %1").arg(window.windowTitle()));
        check(stack->count() == 6,
              QStringLiteral("创建总览页后共 6 个页面，实际 %1").arg(stack->count()));
    }

    check(g_messages.isEmpty(),
          QStringLiteral("运行期间没有 Qt 告警，实际 %1 条%2")
              .arg(g_messages.size())
              .arg(g_messages.isEmpty()
                       ? QString()
                       : QStringLiteral("：") + g_messages.join(QStringLiteral(" | "))));

    report(QStringLiteral("\n===== 结果: %1 =====")
               .arg(g_failures == 0 ? QStringLiteral("全部通过")
                                    : QStringLiteral("失败 %1 项").arg(g_failures)));

    if (g_report != nullptr) {
        std::fclose(g_report);
    }

    // 不进入事件循环，直接结束。
    QTimer::singleShot(0, &application, &QCoreApplication::quit);
    return g_failures == 0 ? 0 : 1;
}
