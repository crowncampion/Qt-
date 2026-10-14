#include "ui/pages/ManageBooksPage.h"

#include "service/LibraryService.h"
#include "ui/dialogs/BookEditDialog.h"
#include "ui_ManageBooksPage.h"

#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>

namespace libms {

ManageBooksPage::ManageBooksPage(LibraryService &service, QWidget *parent)
    : Page(service, parent)
    , ui(new Ui::ManageBooksPage)
    , m_model(new BookTableModel(this))
{
    ui->setupUi(this);

    setupHeader(ui->titleLabel, ui->subtitleLabel, ui->backButton);

    ui->tableView->setModel(m_model);
    // 表格行为：整行选中、不显示行号、按列自适应宽度。
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableView->setAlternatingRowColors(true);
    ui->tableView->verticalHeader()->setVisible(false);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    // 书名可能较长，让书名列占据剩余空间。
    ui->tableView->horizontalHeader()->setSectionResizeMode(BookTableModel::ColumnName,
                                                           QHeaderView::Stretch);

    // 检索方式：显式下拉，替代原先难以发现的滚轮切换。
    ui->searchModeComboBox->addItem(QStringLiteral("按书名（模糊匹配）"),
                                    static_cast<int>(SearchMode::NameKeyword));
    ui->searchModeComboBox->addItem(QStringLiteral("按 ISBN（精确匹配）"),
                                    static_cast<int>(SearchMode::IsbnExact));

    m_isbnValidator = new QRegularExpressionValidator(QRegularExpression(QStringLiteral("^\\d{0,13}$")),
                                                      this);

    connect(ui->searchButton, &QPushButton::clicked, this, &ManageBooksPage::search);
    ui->searchButton->setProperty("primary", true);
    connect(ui->showAllButton, &QPushButton::clicked, this, &ManageBooksPage::showAll);
    connect(ui->editButton, &QPushButton::clicked, this, &ManageBooksPage::editSelected);
    connect(ui->deleteButton, &QPushButton::clicked, this, &ManageBooksPage::removeSelected);
    connect(ui->searchKeywordEdit, &QLineEdit::returnPressed, this, &ManageBooksPage::search);
    connect(ui->searchModeComboBox, &QComboBox::currentIndexChanged, this,
            &ManageBooksPage::handleSearchModeChanged);
    connect(ui->tableView, &QAbstractItemView::doubleClicked, this, &ManageBooksPage::handleDoubleClick);
    connect(ui->tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &ManageBooksPage::updateActionState);

    handleSearchModeChanged(ui->searchModeComboBox->currentIndex());
    showAll();
}

ManageBooksPage::~ManageBooksPage()
{
    delete ui;
}

void ManageBooksPage::refresh()
{
    showAll();
}

SearchMode ManageBooksPage::currentSearchMode() const
{
    const int value = ui->searchModeComboBox->currentData().toInt();
    return static_cast<SearchMode>(value);
}

void ManageBooksPage::handleSearchModeChanged(int index)
{
    Q_UNUSED(index)

    const bool isbnMode = currentSearchMode() == SearchMode::IsbnExact;
    // ISBN 模式只允许数字，避免用户输入汉字后得到空结果却不知原因。
    ui->searchKeywordEdit->setValidator(isbnMode ? m_isbnValidator : nullptr);
    ui->searchKeywordEdit->setPlaceholderText(isbnMode ? QStringLiteral("输入完整的 13 位 ISBN")
                                                      : QStringLiteral("输入书名关键字，留空表示全部"));
    ui->searchKeywordEdit->clear();
}

void ManageBooksPage::search()
{
    const QString keyword = ui->searchKeywordEdit->text().trimmed();

    if (currentSearchMode() == SearchMode::IsbnExact && !keyword.isEmpty()
        && !LibraryService::isValidIsbn(keyword)) {
        notifyError(LibraryService::isbnRuleText(), QStringLiteral("检索条件有误"));
        return;
    }

    const QVector<Book> books = service().searchBooks(currentSearchMode(), keyword);
    m_model->setBooks(books);
    updateActionState();

    if (books.isEmpty()) {
        notify(QStringLiteral("没有找到符合条件的图书"));
    } else {
        notify(QStringLiteral("检索到 %1 种图书").arg(books.size()));
    }
}

void ManageBooksPage::showAll()
{
    ui->searchKeywordEdit->clear();
    m_model->setBooks(service().allBooks());
    updateActionState();
}

Book ManageBooksPage::selectedBook() const
{
    const QModelIndex current = ui->tableView->currentIndex();
    if (!current.isValid()) {
        return Book{};
    }
    return m_model->bookAt(current.row());
}

void ManageBooksPage::updateActionState()
{
    const bool hasSelection = selectedBook().isValid();
    ui->editButton->setEnabled(hasSelection);
    ui->deleteButton->setEnabled(hasSelection);
}

void ManageBooksPage::handleDoubleClick(const QModelIndex &index)
{
    if (index.isValid()) {
        editSelected();
    }
}

void ManageBooksPage::editSelected()
{
    const Book book = selectedBook();
    if (!book.isValid()) {
        notifyError(QStringLiteral("请先在列表中选择一本书"), QStringLiteral("无法修改"));
        return;
    }

    BookEditDialog dialog(this);
    BookEditDialog::BookInput input;
    input.name = book.name;
    input.author = book.author;
    input.total = book.total;
    dialog.prepareForEdit(book.isbn, input);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const BookEditDialog::BookInput edited = dialog.input();

    Book updated = book;
    updated.name = edited.name;
    updated.author = edited.author;
    updated.total = edited.total;

    const VoidResult result = service().updateBook(updated);
    if (!result) {
        notifyError(result.error(), QStringLiteral("无法修改"));
        return;
    }

    notify(QStringLiteral("已更新《%1》的信息").arg(edited.name));
    search();
}

void ManageBooksPage::removeSelected()
{
    const Book book = selectedBook();
    if (!book.isValid()) {
        notifyError(QStringLiteral("请先在列表中选择一本书"), QStringLiteral("无法删除"));
        return;
    }

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this,
        QStringLiteral("确认删除"),
        QStringLiteral("确定要删除《%1》（ISBN：%2）吗？\n此操作不可撤销。").arg(book.name, book.isbn),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (answer != QMessageBox::Yes) {
        return;
    }

    const VoidResult result = service().removeBook(book.id);
    if (!result) {
        notifyError(result.error(), QStringLiteral("无法删除"));
        return;
    }

    notify(QStringLiteral("已删除《%1》").arg(book.name));
    search();
}

} // namespace libms
