#include "ui/widgets/PageHeader.h"

#include <QLabel>
#include <QPushButton>

namespace libms {
namespace PageHeader {

void applyTo(QLabel *titleLabel, QLabel *subtitleLabel, QPushButton *backButton,
             const QString &title, const QString &subtitle)
{
    if (titleLabel != nullptr) {
        titleLabel->setText(title);
        // objectName 供样式表 QLabel#pageTitle 命中，统一各页标题外观。
        titleLabel->setObjectName(QStringLiteral("pageTitle"));
    }
    if (subtitleLabel != nullptr) {
        subtitleLabel->setText(subtitle);
        subtitleLabel->setObjectName(QStringLiteral("pageSubtitle"));
    }
    if (backButton != nullptr) {
        backButton->setText(QStringLiteral("← 返回主菜单"));
        backButton->setCursor(Qt::PointingHandCursor);
    }
}

} // namespace PageHeader
} // namespace libms
