#include "data/Database.h"

#include "core/TimeUtil.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QLoggingCategory>

namespace libms {
namespace {

Q_LOGGING_CATEGORY(lcDatabase, "libms.database")

/// 连接名固定，保证全进程只有一条到业务库的连接。
const QString kConnectionName = QStringLiteral("libms_main");

/// 将 QSqlError 转成可直接展示给用户的中文提示。
QString describe(const QSqlError &error)
{
    const QString driverText = error.driverText().trimmed();
    const QString databaseText = error.databaseText().trimmed();
    if (!databaseText.isEmpty()) {
        return databaseText;
    }
    if (!driverText.isEmpty()) {
        return driverText;
    }
    return QStringLiteral("未知数据库错误");
}

} // namespace

const QString &Database::connectionName()
{
    return kConnectionName;
}

Database::Database() = default;

Database::~Database()
{
    close();
}

bool Database::isOpen() const
{
    return m_open && QSqlDatabase::database(kConnectionName, /*open=*/false).isOpen();
}

QSqlDatabase Database::handle() const
{
    return QSqlDatabase::database(kConnectionName, /*open=*/false);
}

VoidResult Database::open(const DatabaseConfig &config)
{
    close();

    m_config = config;

    // addDatabase 要求连接名尚未被占用；removeDatabase 后需让局部对象先析构，
    // 因此这里用一个独立作用域确保 QSqlDatabase 句柄在 removeDatabase 前失效。
    {
        if (QSqlDatabase::contains(kConnectionName)) {
            QSqlDatabase::removeDatabase(kConnectionName);
        }
    }

    bool opened = false;
    QString failure;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QMYSQL"), kConnectionName);
        db.setHostName(config.hostName);
        db.setPort(config.port);
        db.setUserName(config.userName);
        db.setPassword(config.password);
        db.setDatabaseName(config.databaseName);
        db.setConnectOptions(config.connectOptions());

        opened = db.open();
        if (!opened) {
            failure = describe(db.lastError());
        }
    }

    if (!opened) {
        m_open = false;
        m_lastError = failure;
        qCWarning(lcDatabase) << "连接数据库失败:" << failure;
        return VoidResult::failure(QStringLiteral("无法连接数据库：%1").arg(failure));
    }

    m_open = true;
    qCInfo(lcDatabase) << "已连接数据库" << config.databaseName << "于" << config.hostName;

    const VoidResult migration = migrateSchema();
    if (!migration) {
        m_lastError = migration.error();
        return migration;
    }

    m_lastError.clear();
    return VoidResult::success();
}

void Database::close()
{
    if (QSqlDatabase::contains(kConnectionName)) {
        {
            QSqlDatabase db = QSqlDatabase::database(kConnectionName, /*open=*/false);
            if (db.isOpen()) {
                db.close();
            }
        }
        QSqlDatabase::removeDatabase(kConnectionName);
    }
    m_open = false;
}

bool Database::tableExists(const QString &table) const
{
    QSqlQuery query(handle());
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM information_schema.tables "
                                 "WHERE table_schema = DATABASE() AND table_name = ?"));
    query.addBindValue(table);
    if (!query.exec() || !query.next()) {
        return false;
    }
    return query.value(0).toInt() > 0;
}

bool Database::columnExists(const QString &table, const QString &column) const
{
    QSqlQuery query(handle());
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM information_schema.columns "
                                 "WHERE table_schema = DATABASE() AND table_name = ? "
                                 "AND column_name = ?"));
    query.addBindValue(table);
    query.addBindValue(column);
    if (!query.exec() || !query.next()) {
        return false;
    }
    return query.value(0).toInt() > 0;
}

VoidResult Database::migrateSchema()
{
    QSqlDatabase db = handle();

    // 1) 历史库可能缺少 available 列：补列后按「总量 - 未归还数量」回填。
    if (!columnExists(QStringLiteral("book"), QStringLiteral("available"))) {
        qCInfo(lcDatabase) << "为 book 表新增 available 列";

        QSqlQuery alter(db);
        if (!alter.exec(QStringLiteral("ALTER TABLE book ADD COLUMN available INT NOT NULL DEFAULT 0"))) {
            return VoidResult::failure(
                QStringLiteral("升级表结构失败（book.available）：%1").arg(describe(alter.lastError())));
        }

        // total - 仍在借出的数量，并用 GREATEST 兜底避免出现负数。
        const QString backfill = QStringLiteral(
            "UPDATE book b SET b.available = GREATEST(b.total - COALESCE(("
            "  SELECT SUM(l.borrow_count - l.returned_count) FROM lend_records l"
            "  WHERE l.isbn = b.ISBN AND l.borrow_count > l.returned_count"
            "), 0), 0)");
        QSqlQuery update(db);
        if (!update.exec(backfill)) {
            return VoidResult::failure(
                QStringLiteral("回填可借数量失败：%1").arg(describe(update.lastError())));
        }
        qCInfo(lcDatabase) << "已回填" << update.numRowsAffected() << "条图书的可借数量";
    }

    return VoidResult::success();
}

} // namespace libms
