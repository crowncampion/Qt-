#include "ui/dialogs/ReturnBookDialog.h"

#include "core/TimeUtil.h"
#include "ui_ReturnBookDialog.h"

namespace libms {

ReturnBookDialog::ReturnBookDialog(const LendRecord &record, const QString &bookName, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ReturnBookDialog)
{
    ui->setupUi(this);

    setWindowTitle(QStringLiteral("归还图书"));

    ui->readerValue->setText(record.readerId);
    ui->isbnValue->setText(record.isbn);
    ui->bookNameValue->setText(bookName.isEmpty() ? QStringLiteral("（馆藏中已无此书）") : bookName);
    ui->borrowTimeValue->setText(TimeUtil::format(record.borrowTime));
    ui->borrowedValue->setText(QString::number(record.borrowCount));
    ui->returnedValue->setText(QString::number(record.returnedCount));
    ui->outstandingValue->setText(QString::number(record.outstandingCount()));
    ui->returnTimeValue->setText(TimeUtil::format(TimeUtil::now()));

    // 归还数量上限即未归还数量，从控件层面先杜绝非法输入。
    const int outstanding = record.outstandingCount();
    ui->countSpinBox->setRange(1, qMax(1, outstanding));
    ui->countSpinBox->setValue(outstanding);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, [this] {
        m_returnCount = ui->countSpinBox->value();
        accept();
    });
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

ReturnBookDialog::~ReturnBookDialog()
{
    delete ui;
}

int ReturnBookDialog::returnCount() const
{
    return m_returnCount;
}

} // namespace libms
