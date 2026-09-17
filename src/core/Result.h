#pragma once

#include <QString>
#include <QStringList>

#include <optional>
#include <utility>

namespace libms {

/**
 * @brief 操作结果。
 *
 * 数据层与业务层不直接弹窗，而是返回 Result；由界面层决定如何呈现。
 * 这样同一套业务逻辑既能被 GUI 使用，也能被测试或命令行复用。
 *
 * @code
 * Result<Book> r = service.findBook(isbn);
 * if (!r) { showError(r.error()); } else { use(r.value()); }
 * @endcode
 */
template <typename T>
class Result
{
public:
    /// 构造一个成功结果。
    static Result success(T value) { return Result(std::move(value)); }

    /// 构造一个失败结果，@p message 为面向用户的中文提示。
    static Result failure(QString message) { return Result(std::move(message)); }

    bool isSuccess() const noexcept { return m_value.has_value(); }
    explicit operator bool() const noexcept { return isSuccess(); }

    /// 失败原因；成功时返回空字符串。
    const QString &error() const noexcept { return m_error; }

    const T &value() const & { return *m_value; }
    T &value() & { return *m_value; }
    T &&value() && { return std::move(*m_value); }

    /// 成功时返回内部值，失败时返回 @p fallback。
    T valueOr(T fallback) const { return m_value ? *m_value : std::move(fallback); }

private:
    explicit Result(T value) : m_value(std::move(value)) {}
    explicit Result(QString message) : m_error(std::move(message)) {}

    std::optional<T> m_value;
    QString m_error;
};

/**
 * @brief 无返回值的操作结果。
 *
 * 直接以「错误信息列表」承载失败原因：空列表即成功。
 */
class VoidResult
{
public:
    static VoidResult success() { return VoidResult(); }
    static VoidResult failure(QString message) { return VoidResult(std::move(message)); }

    bool isSuccess() const noexcept { return m_errors.isEmpty(); }
    explicit operator bool() const noexcept { return isSuccess(); }

    /// 面向用户展示的失败原因（可能有多条校验错误）。
    QString error() const { return m_errors.join(QLatin1Char('\n')); }
    const QStringList &errors() const noexcept { return m_errors; }

private:
    VoidResult() = default;
    explicit VoidResult(QString message) { m_errors.append(std::move(message)); }

    QStringList m_errors;
};

} // namespace libms
