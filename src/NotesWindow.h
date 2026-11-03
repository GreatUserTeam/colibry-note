#pragma once

#include "core/NoteStore.h"

#include <QMainWindow>

class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QTimer;

namespace colibry {
class SettingsStore;
}

namespace colibry::notes {

/// Notes window: a searchable list on the left, the text of one note on the right.
/// Typing is saved automatically; nothing has to be confirmed.
class NotesWindow : public QMainWindow
{
    Q_OBJECT

public:
    NotesWindow(NoteStore &notes, SettingsStore &settings, QWidget *parent = nullptr);

    QString currentId() const { return m_currentId; }
    int noteCount() const;
    QStringList visibleIds() const;
    QPlainTextEdit *editor() const { return m_editor; }
    bool hasUnsavedChanges() const { return m_dirty; }
    QString statusText() const;

    QString newNote();
    bool openNote(const QString &id);
    bool deleteCurrent();
    void setSearch(const QString &text);

    /// Saves pending text now. An empty note that is being left is removed instead.
    void flush();

    static constexpr int kSaveDelayMs = 600;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void buildUi();
    void reloadList();
    void onTextEdited();
    void saveNow();
    void leaveCurrent();
    void showEmptyState();
    void updateStatus(const QString &saved = {});
    void promptDelete();

    NoteStore &m_notes;
    SettingsStore &m_settings;

    QLineEdit *m_search = nullptr;
    QListWidget *m_list = nullptr;
    QPlainTextEdit *m_editor = nullptr;
    QLabel *m_status = nullptr;
    QTimer *m_timer = nullptr;

    QString m_currentId;
    bool m_dirty = false;
    bool m_loading = false;
    QString m_lastError;
};

} // namespace colibry::notes
