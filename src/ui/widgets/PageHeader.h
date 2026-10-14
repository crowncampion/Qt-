#pragma once

#include <QString>

QT_BEGIN_NAMESPACE
class QLabel;
class QPushButton;
QT_END_NAMESPACE

namespace libms {

/**
 * @brief 页面标题栏的统一处理。
 *
 * 原先五个页面各自摆放一个「返回」按钮，标题字号与按钮文案靠手工对齐，难以保持一致。
 * 这里提供一个小工具，把 Designer 中已经摆好的控件接管过来，统一文案与样式对象名。
 *
 * 之所以是工具函数而不是自定义控件：各页的标题栏都直接写在各自的 .ui 里，
 * 页面只依赖本函数，不需要额外的控件类与信号转发。
 */
namespace PageHeader {

/**
 * @brief 把一个标题 QLabel、副标题 QLabel 与返回 QPushButton 接入统一行为。
 *
 * @param titleLabel 标题标签，会设置 objectName 为 pageTitle 以命中样式表。
 * @param subtitleLabel 副标题标签，可为 nullptr（主菜单没有返回按钮，但仍有副标题）。
 * @param backButton 返回按钮，可为 nullptr（主菜单不需要返回按钮）。
 * @param title 标题文案。
 * @param subtitle 副标题文案。
 */
void applyTo(QLabel *titleLabel, QLabel *subtitleLabel, QPushButton *backButton,
             const QString &title, const QString &subtitle);

} // namespace PageHeader
} // namespace libms
