#pragma once

#include <QString>
#include <QStringList>

namespace libms {
namespace test {

/**
 * @brief 极简测试框架。
 *
 * 项目规模不大，引入 QtTest 会额外增加一个模块依赖，
 * 因此这里用一个可读性足够、失败即打印上下文的轻量实现。
 */
class TestRunner
{
public:
    /// 断言 @p condition 成立，否则记录一条失败。
    void check(bool condition, const QString &what);

    /// 断言两个值相等，失败时打印期望值与实际值。
    void checkEqual(const QString &actual, const QString &expected, const QString &what);
    void checkEqual(int actual, int expected, const QString &what);

    /// 记录一条错误信息。
    void fail(const QString &message);

    /// 执行一个用例；返回失败数量。
    int run(const QString &suiteName);

    /// 全部失败信息。
    const QStringList &failures() const { return m_failures; }

private:
    QStringList m_failures;
};

} // namespace test
} // namespace libms
