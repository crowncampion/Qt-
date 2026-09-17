#pragma once

#include <QDateTime>
#include <QString>

namespace libms {

/**
 * @brief 时间格式化工具。
 *
 * 数据库中 borrow_time / return_time 为 DATETIME，全项目统一使用
 * kDateTimeFormat 这一种格式，避免各处格式不一致导致字符串比较失效。
 */
namespace TimeUtil {

/// 数据库读写与界面展示共用的时间格式。
inline constexpr const char *kDateTimeFormat = "yyyy-MM-dd HH:mm:ss";

/// 当前时间。
inline QDateTime now()
{
    return QDateTime::currentDateTime();
}

/// 按统一格式序列化；无效时间返回空字符串。
inline QString format(const QDateTime &dateTime)
{
    return dateTime.isValid() ? dateTime.toString(QLatin1String(kDateTimeFormat)) : QString();
}

/// 解析数据库返回的时间字符串；失败时返回无效 QDateTime。
inline QDateTime parse(const QString &text)
{
    return QDateTime::fromString(text, QLatin1String(kDateTimeFormat));
}

} // namespace TimeUtil
} // namespace libms
