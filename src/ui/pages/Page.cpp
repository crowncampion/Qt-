#include "ui/pages/Page.h"

#include "service/LibraryService.h"
#include "ui/widgets/PageHeader.h"

#include <QLabel>
#include <QMessageBox>
#include <QPushButton>

namespace libms {

Page::Page(LibraryService &service, QWidget *parent)
    : QWidget(parent)
    , m_service(service)
{
}

QString Page::pageTitle() const
{
    return m_title;
}

QString Page::pageSubtitle() const
{
    return m_subtitle;
}

void Page::setupHeader(QLabel *titleLabel, QLabel *subtitleLabel, QPushButton *backButton)
{
    if (titleLabel != nullptr) {
        m_title = titleLabel->text();
    }
    if (subtitleLabel != nullptr) {
        m_subtitle = subtitleLabel->text();
    }

    PageHeader::applyTo(titleLabel, subtitleLabel, backButton, m_title, m_subtitle);

    if (backButton != nullptr) {
        connect(backButton, &QPushButton::clicked, this, &Page::backRequested);
    }
}

void Page::notify(const QString &message)
{
    emit statusMessage(message);
}

void Page::notifyError(const QString &message, const QString &title)
{
    QMessageBox::warning(this, title, message);
}

} // namespace libms
