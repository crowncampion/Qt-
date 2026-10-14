#include "TestRunner.h"

#include <QDebug>
#include <QFile>
#include <QTextStream>

namespace libms {
namespace test {
namespace {

/// 结果同时写入控制台与 test-results.txt，便于在无法读取控制台输出时排查。
class Reporter
{
public:
    Reporter()
    {
        m_file.setFileName(QStringLiteral("test-results.txt"));
        // 无法写文件时也不中断测试：控制台仍会输出结果。
        if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            qWarning("无法写入 test-results.txt");
        }
        m_stream.setDevice(&m_file);
    }

    QTextStream &stream() { return m_stream; }

private:
    QFile m_file;
    QTextStream m_stream;
};

Reporter &reporter()
{
    static Reporter instance;
    return instance;
}

QTextStream &out()
{
    return reporter().stream();
}

} // namespace

void TestRunner::check(bool condition, const QString &what)
{
    if (condition) {
        out() << "  [PASS] " << what << "\n";
        out().flush();
        return;
    }
    fail(QStringLiteral("断言失败: %1").arg(what));
}

void TestRunner::checkEqual(const QString &actual, const QString &expected, const QString &what)
{
    if (actual == expected) {
        out() << "  [PASS] " << what << "\n";
        out().flush();
        return;
    }
    fail(QStringLiteral("断言失败: %1（期望 \"%2\"，实际 \"%3\"）").arg(what, expected, actual));
}

void TestRunner::checkEqual(int actual, int expected, const QString &what)
{
    if (actual == expected) {
        out() << "  [PASS] " << what << "\n";
        out().flush();
        return;
    }
    fail(QStringLiteral("断言失败: %1（期望 %2，实际 %3）").arg(what).arg(expected).arg(actual));
}

void TestRunner::fail(const QString &message)
{
    m_failures.append(message);
    out() << "  [FAIL] " << message << "\n";
    out().flush();
}

int TestRunner::run(const QString &suiteName)
{
    out() << "===== " << suiteName << " =====\n";
    out().flush();
    return m_failures.size();
}

} // namespace test
} // namespace libms
