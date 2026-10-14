#pragma once

#include <QString>

namespace libms {

/**
 * @brief MySQL 连接参数。
 *
 * 凭据不再硬编码在源码里，而是来自 config.ini 或环境变量。
 */
struct DatabaseConfig
{
    QString hostName = QStringLiteral("127.0.0.1");
    int port = 3306;
    QString userName = QStringLiteral("root");
    QString password;
    QString databaseName = QStringLiteral("book_system");
    /// 连接超时（秒），避免数据库不可达时界面长时间卡死。
    int connectTimeoutSeconds = 5;

    /// 供 QSqlDatabase::setConnectOptions() 使用的选项串。
    QString connectOptions() const;
};

/**
 * @brief 应用配置读取器。
 *
 * 查找顺序（先命中者生效）：
 *  1. 可执行文件同目录的 config.ini；
 *  2. 自可执行文件目录逐级向上查找的 config.ini（便于在构建目录中直接运行）；
 *  3. 环境变量 LIBMS_DB_HOST / _PORT / _USER / _PASSWORD / _NAME；
 *  4. 代码内置默认值。
 *
 * 缺少配置文件不视为错误：只要默认值能连上数据库，程序照常启动。
 */
class AppConfig
{
public:
    /// 从标准位置加载配置。
    static DatabaseConfig load();

    /// 从指定目录开始向上查找并加载（单元测试用）。
    static DatabaseConfig loadFrom(const QString &startDirectory);

    /// 实际生效的配置文件路径；未找到时为空。
    static QString resolvedConfigPath();
};

} // namespace libms
