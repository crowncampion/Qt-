#pragma once

#include <QWidget>

QT_BEGIN_NAMESPACE
class QLabel;
class QPushButton;
QT_END_NAMESPACE

namespace libms {

class LibraryService;

/**
 * @brief 功能页的公共基类。
 *
 * 提供统一的能力：
 *  - 自动装配页头（标题 + 副标题 + 返回按钮），并转发 backRequested 信号；
 *  - refresh() 钩子，供主窗口在切换到该页时刷新数据；
 *  - notify() / notifyError() 以状态栏或弹窗提示结果。
 *
 * 各页只依赖 LibraryService，不再持有 QSqlDatabase 或 SQL 语句。
 */
class Page : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造页面。
     * @param service 业务服务，生命周期由 MainWindow 保证长于本页。
     * @param parent 父控件。
     */
    explicit Page(LibraryService &service, QWidget *parent = nullptr);

    /// 数据刷新钩子；默认不做任何事，需要重新加载数据的页面覆盖它。
    virtual void refresh() {}

    /// 页面标题。
    QString pageTitle() const;

    /// 页面副标题。
    QString pageSubtitle() const;

signals:
    /// 请求返回主菜单。
    void backRequested();

    /// 需要在状态栏显示一条信息。
    void statusMessage(const QString &message);

protected:
    /**
     * @brief 把 Designer 中的标题控件与返回按钮接入统一的页头行为。
     * @param titleLabel 标题 QLabel。
     * @param subtitleLabel 副标题 QLabel，可为 nullptr。
     * @param backButton 返回按钮。
     */
    void setupHeader(QLabel *titleLabel, QLabel *subtitleLabel, QPushButton *backButton);

    /// 取业务服务。
    LibraryService &service() const { return m_service; }

    /// 在状态栏提示一条成功信息。
    void notify(const QString &message);

    /**
     * @brief 弹出错误提示。
     * @param message 错误详情。
     * @param title 对话框标题。
     */
    void notifyError(const QString &message, const QString &title = QStringLiteral("操作失败"));

    /// 提示成功时使用的对话框标题。
    static QString successTitle() { return QStringLiteral("操作成功"); }

private:
    LibraryService &m_service;
    QString m_title;
    QString m_subtitle;
};

} // namespace libms
