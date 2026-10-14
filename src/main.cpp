#include "ui/MainWindow.h"

#include <QApplication>
#include <QIcon>
#include <QLoggingCategory>
#include <QMessageBox>

/*
 * 启动顺序：创建 QApplication -> 建库连接 -> 显示主窗口。
 * 数据库不可用时给出明确提示后退出，而不是进入一个所有操作都会失败的空界面。
 */
int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("图书馆借阅管理系统"));
    QApplication::setOrganizationName(QStringLiteral("LibraryMS"));
    QApplication::setApplicationVersion(QStringLiteral("2.0"));

    libms::MainWindow window;
    if (const QString error = window.initialize(); !error.isEmpty()) {
        QMessageBox::critical(nullptr,
                              QStringLiteral("启动失败"),
                              QStringLiteral("%1\n\n请检查 config.ini 中的数据库配置，"
                                             "确认 MySQL 服务已启动且账号密码正确。")
                                  .arg(error));
        return 1;
    }

    window.show();
    return QApplication::exec();
}
