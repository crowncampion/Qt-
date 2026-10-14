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

// ---------------------------------------------------------------------------
// 表结构迁移
//
// 每个迁移负责把库从「版本 N-1」推进到「版本 N」，并更新 schema_version。
// 新增迁移的做法：在下面加一个函数，再往 kMigrations 末尾追加一项。
// 控制流不需要改动。
// ---------------------------------------------------------------------------

/// book.available 的列名，供"列是否存在"检查与 ALTER 语句共用。
const QString kAvailableColumn = QStringLiteral("available");

/// 迁移 1：为重构前建立的数据库补上 book.available 列，并回填真实可借数量。
///
/// 旧结构里没有记录可借数量的地方，只能从 lend_records 的借阅流水反推：
///     available = total - SUM(未归还数量)
/// 因此这一步无法由通用机制自动完成，必须手写。
VoidResult migrationAddAvailableColumn(QSqlDatabase &db)
{
    QSqlQuery check(db);
    check.prepare(QStringLiteral("SELECT COUNT(*) FROM information_schema.columns "
                                 "WHERE table_schema = DATABASE() AND table_name = 'book' "
                                 "AND column_name = ?"));
    check.addBindValue(kAvailableColumn);

    bool columnPresent = false;
    if (check.exec() && check.next()) {
        columnPresent = check.value(0).toInt() > 0;
    }

    // 列已存在就直接返回：让这个迁移可以安全地重复执行。
    // （版本号正常会拦住重复调用，这里是为手工重跑或 version 表被清掉的情况兜底。）
    if (columnPresent) {
        return VoidResult::success();
    }

    qCInfo(lcDatabase) << "迁移 1：为 book 表新增 available 列";

    // 加列时已有行必须有个值，所以先给默认 0，下面再把真实值算出来覆盖。
    QSqlQuery alter(db);
    if (!alter.exec(QStringLiteral("ALTER TABLE book ADD COLUMN available INT NOT NULL DEFAULT 0"))) {
        return VoidResult::failure(
            QStringLiteral("升级表结构失败（book.available）：%1").arg(describe(alter.lastError())));
    }

    // 回填：total 减去仍在借出的数量；GREATEST 兜底避免历史数据造成负数。
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
    return VoidResult::success();
}

/// 迁移表：按 version 升序排列，version 必须从 1 开始且连续。
struct SchemaMigration
{
    int version;
    const char *description;
    VoidResult (*apply)(QSqlDatabase &);
};

const SchemaMigration kMigrations[] = {
    { 1, "新增 book.available 并回填可借数量", &migrationAddAvailableColumn },
};

/// 最新版本 = 迁移条数（version 从 1 开始连续编号，所以两者相等）。
int latestSchemaVersion()
{
    return static_cast<int>(sizeof(kMigrations) / sizeof(kMigrations[0]));
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
    // 把对象和 Qt 全局连接表都恢复到"未连接"状态，使本次调用与调用前的状态无关：
    // 无论是首次连接、重复调用，还是上一次 open() 失败后残留了连接名，都能从这里重新开始。
    close();

    m_config = config;

    // 连接结果先记在局部变量里，不在尝试过程中改动对象状态；
    // 等下面这个作用域结束、db 析构后，再统一提交或拒绝。
    bool opened = false;
    QString failure;
    {
        // 独立作用域：db 必须在本次调用返回【之前】析构。
        // 只要还有 QSqlDatabase 句柄引用着这条连接，后续 close() 里的
        // removeDatabase 就会报 "connection ... is still in use"，
        // 且该句柄会变成悬空引用。
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
        // 注意这里用 opened（本次 db.open() 的结果）而不是 m_open 作判断：
        // opened 是本次尝试的新鲜结果，m_open 是"对象已提交的状态"，可能还是上次的值。
        // 用 m_open 承接也能工作，但那样会让同一个成员同时扮演"探针"和"状态"两个角色。
        m_open = false;
        m_lastError = failure;      // 供界面取用（lastError()）
        qCWarning(lcDatabase) << "连接数据库失败:" << failure;
        return VoidResult::failure(QStringLiteral("无法连接数据库：%1").arg(failure));
    }

    // 到此确认连接可用，才提交状态。表结构升级紧随其后。
    m_open = true;
    qCInfo(lcDatabase) << "已连接数据库" << config.databaseName << "于" << config.hostName;

    const VoidResult migration = migrateSchema();
    if (!migration) {
        // 迁移失败：连接本身是通的，但按"open() 成功才代表可用"的约定，
        // 这里必须把状态也退回未打开，避免返回失败对象却报告已连接。
        // 连接仍留在 Qt 全局表中，需要时可用 handle() 查库诊断；close() 会负责清理。
        m_open = false;
        m_lastError = migration.error();
        return migration;
    }

    m_lastError.clear();            // 成功时清掉上一次的残留错误
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

int Database::readSchemaVersion()
{
    QSqlQuery create(handle());
    // 没有这张表说明还没记录过任何版本，等效于版本 0（下面直接返回 0）。
    create.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS schema_version (version INT NOT NULL)"));

    QSqlQuery query(handle());
    if (!query.exec(QStringLiteral("SELECT MAX(version) FROM schema_version"))) {
        return 0;
    }
    if (!query.next()) {
        return 0;
    }

    // 表是空的（刚建出来）：MAX 返回 NULL，toInt() 得到 0，正好表示"还没迁移过"。
    return query.value(0).toInt();
}

VoidResult Database::writeSchemaVersion(int version)
{
    // 表里始终只保留一行。
    // 注意：这里靠"影响行数为 0"判断"还没有记录"。MySQL 对"值未变化"的 UPDATE
    // 也返回 0 行，所以必须配合调用方的约定 —— 只会写入比当前更大的版本号，
    // 从而保证有记录时 UPDATE 一定真的改动了值。
    QSqlQuery update(handle());
    update.prepare(QStringLiteral("UPDATE schema_version SET version = :version"));
    update.bindValue(QStringLiteral(":version"), version);
    if (!update.exec()) {
        return VoidResult::failure(
            QStringLiteral("记录表结构版本失败：%1").arg(describe(update.lastError())));
    }
    if (update.numRowsAffected() > 0) {
        return VoidResult::success();
    }

    QSqlQuery insert(handle());
    insert.prepare(QStringLiteral("INSERT INTO schema_version (version) VALUES (:version)"));
    insert.bindValue(QStringLiteral(":version"), version);
    if (!insert.exec()) {
        return VoidResult::failure(
            QStringLiteral("记录表结构版本失败：%1").arg(describe(insert.lastError())));
    }
    return VoidResult::success();
}

VoidResult Database::migrateSchema()
{
    QSqlDatabase db = handle();

    const int current = readSchemaVersion();
    const int latest = latestSchemaVersion();
    if (current >= latest) {
        return VoidResult::success();
    }

    qCInfo(lcDatabase) << "表结构版本" << current << "，待应用至" << latest;

    for (int i = current; i < latest; ++i) {
        const SchemaMigration &migration = kMigrations[i];
        qCInfo(lcDatabase) << "应用迁移" << migration.version << migration.description;

        const VoidResult applied = migration.apply(db);
        if (!applied) {
            return VoidResult::failure(
                QStringLiteral("应用迁移 %1（%2）失败：%3")
                    .arg(migration.version)
                    .arg(QString::fromUtf8(migration.description))
                    .arg(applied.error()));
        }

        // 每个迁移成功后就记一次版本，中途失败时下次只重跑未完成的部分。
        const VoidResult recorded = writeSchemaVersion(migration.version);
        if (!recorded) {
            return recorded;
        }
    }

    return VoidResult::success();
}

} // namespace libms
