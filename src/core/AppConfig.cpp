#include "core/AppConfig.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStringList>

namespace libms {
namespace {

/// config.ini 的文件名。
constexpr auto kConfigFileName = "config.ini";

/// 向上查找的最大层数，防止在异常目录结构下无限循环。
constexpr int kMaxSearchDepth = 4;

QString readEnv(const char *name)
{
    return qEnvironmentVariable(name).trimmed();
}

/**
 * @brief 从 @p directory 起逐级向上查找 config.ini。
 */
QString findConfigFile(const QString &directory)
{
    QDir dir(directory);
    for (int depth = 0; depth <= kMaxSearchDepth; ++depth) {
        const QString candidate = dir.absoluteFilePath(QLatin1String(kConfigFileName));
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
        if (!dir.cdUp()) {
            break;
        }
    }
    return QString();
}

/**
 * @brief 用环境变量覆盖已加载的配置。
 *
 * 便于在不修改 config.ini 的前提下临时切换数据库（例如 CI 或测试库）。
 */
void applyEnvironmentOverrides(DatabaseConfig &config)
{
    if (const QString host = readEnv("LIBMS_DB_HOST"); !host.isEmpty()) {
        config.hostName = host;
    }
    if (const QString port = readEnv("LIBMS_DB_PORT"); !port.isEmpty()) {
        bool ok = false;
        const int value = port.toInt(&ok);
        if (ok && value > 0 && value <= 65535) {
            config.port = value;
        }
    }
    if (const QString user = readEnv("LIBMS_DB_USER"); !user.isEmpty()) {
        config.userName = user;
    }
    // 密码允许被显式置空，因此这里只判断变量是否存在。
    if (qEnvironmentVariableIsSet("LIBMS_DB_PASSWORD")) {
        config.password = qEnvironmentVariable("LIBMS_DB_PASSWORD");
    }
    if (const QString name = readEnv("LIBMS_DB_NAME"); !name.isEmpty()) {
        config.databaseName = name;
    }
}

} // namespace

QString DatabaseConfig::connectOptions() const
{
    return QStringLiteral("MYSQL_OPT_CONNECT_TIMEOUT=%1").arg(connectTimeoutSeconds);
}

QString AppConfig::resolvedConfigPath()
{
    return findConfigFile(QCoreApplication::applicationDirPath());
}

DatabaseConfig AppConfig::load()
{
    return loadFrom(QCoreApplication::applicationDirPath());
}

DatabaseConfig AppConfig::loadFrom(const QString &startDirectory)
{
    DatabaseConfig config;

    const QString configFile = findConfigFile(startDirectory);
    if (!configFile.isEmpty()) {
        QSettings settings(configFile, QSettings::IniFormat);
        settings.beginGroup(QStringLiteral("Database"));

        config.hostName = settings.value(QStringLiteral("HostName"), config.hostName).toString();
        config.port = settings.value(QStringLiteral("Port"), config.port).toInt();
        config.userName = settings.value(QStringLiteral("UserName"), config.userName).toString();
        config.password = settings.value(QStringLiteral("Password"), config.password).toString();
        config.databaseName =
            settings.value(QStringLiteral("DatabaseName"), config.databaseName).toString();
        config.connectTimeoutSeconds =
            settings.value(QStringLiteral("ConnectTimeout"), config.connectTimeoutSeconds).toInt();

        settings.endGroup();
    }

    applyEnvironmentOverrides(config);
    return config;
}

} // namespace libms
