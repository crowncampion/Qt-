#include "TestRunner.h"

#include "core/AppConfig.h"
#include "core/Result.h"
#include "data/BookRepository.h"
#include "data/Database.h"
#include "data/LoanRepository.h"
#include "service/LibraryService.h"

#include <QCoreApplication>
#include <QSqlQuery>
#include <QTextStream>
#include <QVector>

using namespace libms;

namespace {

/// 测试用 ISBN，避免与真实数据冲突（9 开头为测试专用段）。
const QString kTestIsbn1 = QStringLiteral("9990000000001");
const QString kTestIsbn2 = QStringLiteral("9990000000002");
const QString kTestReader = QStringLiteral("测试读者");

/// 清理本测试可能遗留的数据，保证可重复运行。
void cleanup(Database &database, LibraryService &service)
{
    for (const QString &isbn : {kTestIsbn1, kTestIsbn2}) {
        const Result<Book> book = service.findBookByIsbn(isbn);
        if (!book) {
            continue;
        }
        // 先删除借阅/归还记录（return_records 由外键级联删除）。
        QSqlQuery query(database.handle());
        query.prepare(QStringLiteral("DELETE FROM lend_records WHERE isbn = :isbn"));
        query.bindValue(QStringLiteral(":isbn"), isbn);
        query.exec();
        service.removeBook(book.value().id);
    }
}

/// 查询可借数量，用于验证库存增减。
int availableOf(LibraryService &service, const QString &isbn)
{
    const Result<Book> book = service.findBookByIsbn(isbn);
    return book ? book.value().available : -1;
}

void testIsbnValidation(test::TestRunner &runner)
{
    runner.check(LibraryService::isValidIsbn(QStringLiteral("9787115428028")), QStringLiteral("13 位数字合法"));
    runner.check(!LibraryService::isValidIsbn(QStringLiteral("978711542802")), QStringLiteral("12 位被拒绝"));
    runner.check(!LibraryService::isValidIsbn(QStringLiteral("97871154280289")), QStringLiteral("14 位被拒绝"));
    runner.check(!LibraryService::isValidIsbn(QStringLiteral("97871154280ab")), QStringLiteral("含字母被拒绝"));
    runner.check(!LibraryService::isValidIsbn(QString()), QStringLiteral("空 ISBN 被拒绝"));
}

void testReaderValidation(test::TestRunner &runner)
{
    runner.check(LibraryService::isValidReaderId(QStringLiteral("张三")), QStringLiteral("汉字姓名合法"));
    runner.check(LibraryService::isValidReaderId(QStringLiteral("Alice")), QStringLiteral("英文姓名合法"));
    runner.check(!LibraryService::isValidReaderId(QStringLiteral("张")), QStringLiteral("单字姓名被拒绝"));
    runner.check(!LibraryService::isValidReaderId(QStringLiteral("张三123")), QStringLiteral("含数字被拒绝"));
    runner.check(!LibraryService::isValidReaderId(QStringLiteral("张三!")), QStringLiteral("含符号被拒绝"));
}

void testAddBook(LibraryService &service, test::TestRunner &runner)
{
    runner.check(!service.addBook(QStringLiteral("测试书"), QStringLiteral("作者"), QStringLiteral("123"),
                                 5),
                 QStringLiteral("非法 ISBN 无法录入"));
    runner.check(!service.addBook(QString(), QStringLiteral("作者"), kTestIsbn1, 5),
                 QStringLiteral("空书名无法录入"));
    runner.check(!service.addBook(QStringLiteral("测试书"), QString(), kTestIsbn1, 5),
                 QStringLiteral("空作者无法录入"));
    runner.check(!service.addBook(QStringLiteral("测试书"), QStringLiteral("作者"), kTestIsbn1, 0),
                 QStringLiteral("馆藏总量 0 无法录入"));

    const VoidResult added =
        service.addBook(QStringLiteral("测试书籍甲"), QStringLiteral("测试作者"), kTestIsbn1, 3);
    runner.check(added.isSuccess(), QStringLiteral("合法图书录入成功: %1").arg(added.error()));
    runner.checkEqual(availableOf(service, kTestIsbn1), 3, QStringLiteral("新书可借数量等于馆藏总量"));

    const VoidResult duplicate =
        service.addBook(QStringLiteral("重复书"), QStringLiteral("作者"), kTestIsbn1, 1);
    runner.check(!duplicate, QStringLiteral("重复 ISBN 被拒绝"));
    runner.check(duplicate.error().contains(QStringLiteral("已存在")),
                 QStringLiteral("重复 ISBN 的提示可读"));
}

void testUpdateBook(LibraryService &service, test::TestRunner &runner)
{
    const Result<Book> book = service.findBookByIsbn(kTestIsbn1);
    if (!book) {
        runner.fail(QStringLiteral("无法读取测试图书，跳过修改用例"));
        return;
    }

    Book updated = book.value();
    updated.name = QStringLiteral("测试书籍甲（改名）");
    updated.total = 5;
    runner.check(service.updateBook(updated).isSuccess(), QStringLiteral("修改书名与总量成功"));
    runner.checkEqual(availableOf(service, kTestIsbn1), 5, QStringLiteral("扩容后全部可借"));

    // 借出 2 本后再把总量改到 1 本：应被拒绝（不能小于已借出数量）。
    runner.check(service.lendBook(kTestIsbn1, kTestReader, 2).isSuccess(),
                 QStringLiteral("借出 2 本成功"));
    runner.checkEqual(availableOf(service, kTestIsbn1), 3, QStringLiteral("借出后可借数量减少"));

    Book shrunk = book.value();
    shrunk.total = 1;
    const VoidResult result = service.updateBook(shrunk);
    runner.check(!result, QStringLiteral("总量小于已借出数量时被拒绝"));
    runner.check(result.error().contains(QStringLiteral("不能小于")),
                 QStringLiteral("拒绝原因可读: %1").arg(result.error()));

    // 恢复数据供后续用例使用。
    Book restored = book.value();
    restored.total = 5;
    service.updateBook(restored);
}

void testLendAndReturn(LibraryService &service, test::TestRunner &runner)
{
    runner.check(!service.lendBook(kTestIsbn1, kTestReader, 99), QStringLiteral("超量借出被拒绝"));
    runner.check(!service.lendBook(kTestIsbn1, QStringLiteral("张"), 1),
                 QStringLiteral("非法借阅人姓名被拒绝"));
    runner.check(!service.lendBook(QStringLiteral("000"), kTestReader, 1),
                 QStringLiteral("非法 ISBN 被拒绝"));

    const int before = availableOf(service, kTestIsbn1);

    // 找到刚借出的那条记录（未归还的）。
    QVector<LendRecord> outstanding = service.outstandingLoans();
    int targetId = 0;
    for (const LendRecord &record : outstanding) {
        if (record.isbn == kTestIsbn1 && record.readerId == kTestReader) {
            targetId = record.id;
        }
    }
    runner.check(targetId > 0, QStringLiteral("能查到未归还的借阅记录"));

    LendRecord target;
    for (const LendRecord &record : outstanding) {
        if (record.id == targetId) {
            target = record;
        }
    }
    runner.checkEqual(target.outstandingCount(), 2, QStringLiteral("未归还数量为 2"));

    runner.check(!service.returnBook(targetId, 0), QStringLiteral("归还 0 本被拒绝"));
    runner.check(!service.returnBook(targetId, 3), QStringLiteral("归还超过未还数量被拒绝"));

    runner.check(service.returnBook(targetId, 1).isSuccess(), QStringLiteral("归还 1 本成功"));
    runner.checkEqual(availableOf(service, kTestIsbn1), before + 1,
                      QStringLiteral("归还后可借数量回冲 1"));

    runner.check(service.returnBook(targetId, 1).isSuccess(), QStringLiteral("再归还 1 本成功"));
    runner.checkEqual(availableOf(service, kTestIsbn1), before + 2,
                      QStringLiteral("归还后可借数量回冲 2"));

    runner.check(!service.returnBook(targetId, 1), QStringLiteral("已还清后再次归还被拒绝"));

    // 归还记录应写入 2 条。
    const QVector<ReturnRecord> returns = service.allReturns();
    int matchCount = 0;
    for (const ReturnRecord &record : returns) {
        if (record.borrowId == targetId) {
            ++matchCount;
        }
    }
    runner.checkEqual(matchCount, 2, QStringLiteral("归还流水写入 2 条"));
}

void testSearch(LibraryService &service, test::TestRunner &runner)
{
    const QVector<Book> byName =
        service.searchBooks(SearchMode::NameKeyword, QStringLiteral("测试书籍甲"));
    runner.check(!byName.isEmpty(), QStringLiteral("按书名模糊检索有结果"));

    const QVector<Book> byIsbn = service.searchBooks(SearchMode::IsbnExact, kTestIsbn1);
    runner.checkEqual(byIsbn.size(), 1, QStringLiteral("按 ISBN 精确检索命中 1 条"));

    // 用一个确保不存在的 ISBN，避免与真实馆藏数据冲突。
    const QVector<Book> none =
        service.searchBooks(SearchMode::IsbnExact, QStringLiteral("9999999999999"));
    runner.checkEqual(none.size(), 0, QStringLiteral("不存在的 ISBN 命中 0 条"));

    const QVector<Book> all = service.searchBooks(SearchMode::NameKeyword, QString());
    runner.check(all.size() >= 2, QStringLiteral("空关键字返回全部图书"));
}

void testRemoveBook(LibraryService &service, test::TestRunner &runner)
{
    // 制造一条未归还记录，验证删除保护。
    runner.check(service.addBook(QStringLiteral("测试书籍乙"), QStringLiteral("测试作者"), kTestIsbn2, 2)
                     .isSuccess(),
                 QStringLiteral("录入第二本测试图书"));
    runner.check(service.lendBook(kTestIsbn2, kTestReader, 1).isSuccess(),
                 QStringLiteral("为第二本书建立未归还记录"));

    const Result<Book> book = service.findBookByIsbn(kTestIsbn2);
    if (!book) {
        runner.fail(QStringLiteral("无法读取第二本测试图书"));
        return;
    }

    const VoidResult blocked = service.removeBook(book.value().id);
    runner.check(!blocked, QStringLiteral("存在未归还记录时拒绝删除"));
    runner.check(blocked.error().contains(QStringLiteral("未归还")),
                 QStringLiteral("拒绝原因可读: %1").arg(blocked.error()));
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);

    test::TestRunner runner;
    QTextStream console(stdout);

    const DatabaseConfig config = AppConfig::load();
    console << "数据库: " << config.userName << '@' << config.hostName << ':' << config.port << '/'
            << config.databaseName << "\n";
    console.flush();

    Database database;
    const VoidResult opened = database.open(config);
    if (!opened) {
        console << "无法连接数据库，测试中止: " << opened.error() << "\n";
        console.flush();
        return 2;
    }

    BookRepository bookRepository(database);
    LoanRepository loanRepository(database);
    LibraryService service(bookRepository, loanRepository);

    cleanup(database, service);

    runner.run(QStringLiteral("LibraryService 业务规则测试"));

    console << "== 单元校验 ==\n";
    testIsbnValidation(runner);
    testReaderValidation(runner);

    console << "== 集成流程 ==\n";
    testAddBook(service, runner);
    testUpdateBook(service, runner);
    testLendAndReturn(service, runner);
    testSearch(service, runner);
    testRemoveBook(service, runner);

    // 清理：删除未归还记录后即可删除图书。
    {
        QSqlQuery query(database.handle());
        query.exec(QStringLiteral("DELETE FROM lend_records WHERE isbn IN ('%1','%2')")
                       .arg(kTestIsbn1, kTestIsbn2));
    }
    cleanup(database, service);

    const int failed = runner.failures().size();
    console << "\n===== 结果: " << (failed == 0 ? QStringLiteral("全部通过")
                                               : QStringLiteral("失败 %1 项").arg(failed))
            << " =====\n";
    for (const QString &failure : runner.failures()) {
        console << " - " << failure << "\n";
    }
    console.flush();

    database.close();
    return failed == 0 ? 0 : 1;
}
