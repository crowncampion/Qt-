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
 *  2. 保证表结构满足程序预期。
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
     * 调用前会先关闭已有连接，因此可安全重复调用：无论之前处于何种状态
     * （从未连接、已连接、或上次失败后残留了连接名），调用后的状态只取决于本次尝试。
     *
     * 连接成功后立即执行表结构升级；任一步失败都会返回可展示的错误信息，
     * 并让 isOpen() 返回 false（即使底层连接已经建立）。
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
    /**
     * @brief 按版本号执行待应用的表结构迁移。
     *
     * schema_version 表记录当前已应用到哪个版本（没有这张表、或表为空都视为 0），
     * 然后按顺序执行版本号更高的迁移。每个迁移自身必须可重复执行 ——
     * 版本号只提供"跳过"的捷径，不代替迁移内部的幂等判断（例如列是否已存在）。
     */
    VoidResult migrateSchema();

    /// 读取 schema_version；表不存在或为空都返回 0，表示尚未记录任何版本。
    int readSchemaVersion();

    /// 把已应用到的版本写入 schema_version。
    VoidResult writeSchemaVersion(int version);

    bool m_open = false;
    DatabaseConfig m_config;
    QString m_lastError;
};

} // namespace libms
