#include "NotesWindow.h"

#include "colibry/Keys.h"
#include "colibry/SettingsStore.h"

#include <QAction>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>

namespace colibry::notes {

namespace {
constexpr int kIdRole = Qt::UserRole + 1;
}

NotesWindow::NotesWindow(NoteStore &notes, SettingsStore &settings, QWidget *parent)
    : QMainWindow(parent)
    , m_notes(notes)
    , m_settings(settings)
{
    setWindowTitle(tr("Notes"));
    setWindowIcon(QIcon::fromTheme(QStringLiteral("accessories-text-editor")));
    resize(880, 580);
    setMinimumSize(480, 300);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(kSaveDelayMs);
    connect(m_timer, &QTimer::timeout, this, [this] { saveNow(); });

    buildUi();

    const QByteArray geometry = m_settings.value(Keys::NotesGeometry).toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);

    reloadList();
    if (m_list->count() > 0)
        openNote(m_list->item(0)->data(kIdRole).toString());
    else
        showEmptyState();
}

void NotesWindow::buildUi()
{
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);

    auto *left = new QWidget(splitter);
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(8, 8, 4, 8);
    m_search = new QLineEdit(left);
    m_search->setPlaceholderText(tr("Search notes (Ctrl+F)"));
    m_search->setClearButtonEnabled(true);
    leftLayout->addWidget(m_search);
    m_list = new QListWidget(left);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setAlternatingRowColors(true);
    m_list->setUniformItemSizes(true);
    m_list->setTextElideMode(Qt::ElideRight);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    leftLayout->addWidget(m_list, 1);
    left->setMinimumWidth(190);

    auto *right = new QWidget(splitter);
    auto *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(4, 8, 8, 8);
    m_editor = new QPlainTextEdit(right);
    m_editor->setFrameShape(QFrame::NoFrame);
    m_editor->setPlaceholderText(tr("Start typing. Your note is saved automatically."));
    QFont font = m_editor->font();
    font.setPointSizeF(font.pointSizeF() + 1.0);
    m_editor->setFont(font);
    rightLayout->addWidget(m_editor, 1);

    splitter->addWidget(left);
    splitter->addWidget(right);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({260, 620});
    setCentralWidget(splitter);

    m_status = new QLabel(this);
    statusBar()->addWidget(m_status, 1);

    // Actions
    auto *newAction = new QAction(tr("New Note"), this);
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, [this] { newNote(); m_editor->setFocus(); });

    auto *deleteAction = new QAction(tr("Delete Note…"), this);
    deleteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Backspace));
    connect(deleteAction, &QAction::triggered, this, [this] { promptDelete(); });

    auto *findAction = new QAction(tr("Search"), this);
    findAction->setShortcut(QKeySequence::Find);
    connect(findAction, &QAction::triggered, this, [this] {
        m_search->setFocus();
        m_search->selectAll();
    });

    QMenu *file = menuBar()->addMenu(tr("&Note"));
    file->addAction(newAction);
    file->addAction(deleteAction);
    file->addSeparator();
    QAction *quit = file->addAction(tr("Close"));
    quit->setShortcut(QKeySequence::Close);
    connect(quit, &QAction::triggered, this, &QWidget::close);
    QMenu *edit = menuBar()->addMenu(tr("&Edit"));
    edit->addAction(findAction);
    addAction(newAction);
    addAction(deleteAction);
    addAction(findAction);

    connect(m_search, &QLineEdit::textChanged, this, [this](const QString &text) { setSearch(text); });
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        const QString id = item->data(kIdRole).toString();
        if (id != m_currentId)
            openNote(id);
    });
    connect(m_list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *item) {
        if (item && item->data(kIdRole).toString() != m_currentId)
            openNote(item->data(kIdRole).toString());
    });
    connect(m_editor, &QPlainTextEdit::textChanged, this, [this] { onTextEdited(); });
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

int NotesWindow::noteCount() const
{
    return static_cast<int>(m_notes.list().size());
}

QStringList NotesWindow::visibleIds() const
{
    QStringList ids;
    for (int i = 0; i < m_list->count(); ++i)
        ids << m_list->item(i)->data(kIdRole).toString();
    return ids;
}

void NotesWindow::reloadList()
{
    const QString keep = m_currentId;
    const QSignalBlocker blocker(m_list);
    m_list->clear();

    const QLocale locale;
    const QList<NoteInfo> notes = m_notes.list(m_search->text());
    for (const NoteInfo &info : notes) {
        auto *item = new QListWidgetItem(m_list);
        const QString when = locale.toString(info.modified, QLocale::ShortFormat);
        item->setText(info.preview.isEmpty() ? QStringLiteral("%1\n%2").arg(info.title, when)
                                             : QStringLiteral("%1\n%2 · %3").arg(info.title, when, info.preview));
        item->setToolTip(info.title);
        item->setData(kIdRole, info.id);
        if (info.id == keep)
            m_list->setCurrentItem(item);
    }
}

void NotesWindow::showEmptyState()
{
    m_loading = true;
    m_editor->clear();
    m_loading = false;
    m_editor->setEnabled(false);
    m_currentId.clear();
    m_dirty = false;
    updateStatus();
    statusBar()->showMessage(tr("No notes yet. Press Ctrl+N to write the first one."));
}

QString NotesWindow::newNote()
{
    leaveCurrent();

    const QString id = m_notes.create();
    if (id.isEmpty()) {
        statusBar()->showMessage(tr("Cannot create a note in %1").arg(m_notes.directory()), 6000);
        return {};
    }

    // A search that would hide the new empty note makes no sense any more.
    if (!m_search->text().isEmpty()) {
        const QSignalBlocker blocker(m_search);
        m_search->clear();
    }
    m_currentId = id;
    reloadList();
    openNote(id);
    return id;
}

bool NotesWindow::openNote(const QString &id)
{
    if (id != m_currentId)
        leaveCurrent();

    const NoteStore::Loaded loaded = m_notes.load(id);
    if (!loaded.ok) {
        statusBar()->showMessage(tr("Cannot open the note: %1").arg(loaded.error), 6000);
        reloadList();
        return false;
    }

    m_currentId = id;
    m_loading = true;
    m_editor->setPlainText(loaded.text);
    m_loading = false;
    m_editor->setEnabled(true);
    m_dirty = false;
    m_timer->stop();

    // Select it in the list without triggering another open.
    {
        const QSignalBlocker blocker(m_list);
        for (int i = 0; i < m_list->count(); ++i) {
            if (m_list->item(i)->data(kIdRole).toString() == id) {
                m_list->setCurrentRow(i);
                break;
            }
        }
    }
    statusBar()->clearMessage();
    updateStatus();
    setWindowTitle(tr("%1 — Notes").arg(NoteStore::titleOf(loaded.text)));
    return true;
}

// ---------------------------------------------------------------------------
// Saving
// ---------------------------------------------------------------------------

void NotesWindow::onTextEdited()
{
    if (m_loading || m_currentId.isEmpty())
        return;
    m_dirty = true;
    m_timer->start();
    updateStatus();
}

void NotesWindow::saveNow()
{
    m_timer->stop();
    if (!m_dirty || m_currentId.isEmpty())
        return;

    const NoteStore::Result result = m_notes.save(m_currentId, m_editor->toPlainText());
    if (!result.ok) {
        // Keep the text and the dirty flag: it will be retried and the person is told.
        m_lastError = result.error;
        statusBar()->showMessage(tr("Could not save: %1").arg(result.error), 8000);
        m_timer->start(); // try again
        updateStatus();
        return;
    }

    m_lastError.clear();
    m_dirty = false;
    reloadList();
    setWindowTitle(tr("%1 — Notes").arg(NoteStore::titleOf(m_editor->toPlainText())));
    updateStatus();
}

void NotesWindow::flush()
{
    saveNow();
}

void NotesWindow::leaveCurrent()
{
    if (m_currentId.isEmpty())
        return;
    saveNow();

    // Nobody wants a pile of empty notes.
    if (m_editor->toPlainText().trimmed().isEmpty() && !m_dirty) {
        m_notes.remove(m_currentId);
        m_currentId.clear();
        reloadList();
    }
}

bool NotesWindow::deleteCurrent()
{
    if (m_currentId.isEmpty())
        return false;
    m_timer->stop();
    m_dirty = false;
    const bool ok = m_notes.remove(m_currentId);
    m_currentId.clear();
    reloadList();

    if (m_list->count() > 0)
        openNote(m_list->item(0)->data(kIdRole).toString());
    else
        showEmptyState();
    return ok;
}

void NotesWindow::promptDelete()
{
    if (m_currentId.isEmpty())
        return;
    const auto answer = QMessageBox::warning(
        this, tr("Delete Note"), tr("Delete \"%1\"? This cannot be undone.").arg(NoteStore::titleOf(m_editor->toPlainText())),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer == QMessageBox::Yes)
        deleteCurrent();
}

void NotesWindow::setSearch(const QString &text)
{
    if (m_search->text() != text) {
        const QSignalBlocker blocker(m_search);
        m_search->setText(text);
    }
    flush(); // the list is rebuilt from the files, so pending text must be on disk first
    reloadList();
}

// ---------------------------------------------------------------------------

QString NotesWindow::statusText() const
{
    if (m_currentId.isEmpty())
        return {};
    const QString text = m_editor->toPlainText();
    const QString state = !m_lastError.isEmpty() ? tr("not saved") : (m_dirty ? tr("saving…") : tr("saved"));
    return tr("%1 words · %2 characters · %3")
        .arg(NoteStore::wordCount(text))
        .arg(text.size())
        .arg(state);
}

void NotesWindow::updateStatus(const QString &)
{
    m_status->setText(statusText());
}

void NotesWindow::closeEvent(QCloseEvent *event)
{
    leaveCurrent();
    m_settings.setValue(Keys::NotesGeometry, saveGeometry());
    QMainWindow::closeEvent(event);
}

} // namespace colibry::notes
