#pragma once

#include "core/AppConfig.h"
#include "core/Result.h"

#include <QSqlDatabase>
#include <QString>

namespace libms {

/**
 * @brief 数据库连接的持有者与表结构管理者。
 *
 * 职责被限制为两件事：
 *  1. 按 DatabaseConfig 打开/关闭一条具名连接；
 *  2. 保证表结构满足程序预期（幂等升级）。
 *
 * 所有业务 SQL 都在各 Repository 中，本类不包含任何业务查询。
 */
class Database
{
public:
    /// 连接名，供 QSqlDatabase::database(name) 取用。
    static const QString &connectionName();

    Database();
    ~Database();

    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    /**
     * @brief 建立连接。
     *
     * 连接成功后立即执行表结构升级；任一步失败都会返回可展示的错误信息。
     */
    VoidResult open(const DatabaseConfig &config = AppConfig::load());

    /// 关闭连接并释放连接名。
    void close();

    bool isOpen() const;

    /// 当前连接；未打开时返回的 QSqlDatabase 无效。
    QSqlDatabase handle() const;

    /// 最近一次失败原因，用于界面提示。
    QString lastError() const { return m_lastError; }

    /// 当前连接使用的配置，供状态栏等信息展示。
    const DatabaseConfig &config() const { return m_config; }

private:
    /// 幂等升级表结构：补齐缺失的表与列，并按借阅记录回填可借数量。
    VoidResult migrateSchema();

    /// 检查某张表是否存在指定列。
    bool columnExists(const QString &table, const QString &column) const;

    /// 检查某张表是否存在。
    bool tableExists(const QString &table) const;

    bool m_open = false;
    DatabaseConfig m_config;
    QString m_lastError;
};

} // namespace libms
