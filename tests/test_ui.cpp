#include "NotesWindow.h"
#include "colibry/Keys.h"
#include "colibry/SettingsStore.h"

#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QtTest>

using namespace colibry;
using namespace colibry::notes;

class NotesUiTest : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        QVERIFY(m_tmp.isValid());
        QDir(m_tmp.path()).removeRecursively();
        QDir().mkpath(m_tmp.path());
        m_store = std::make_unique<NoteStore>(m_tmp.path() + QStringLiteral("/notes"));
        m_settings = std::make_unique<SettingsStore>(m_tmp.path() + QStringLiteral("/s.ini"));
    }

    void startsEmptyWithDisabledEditor()
    {
        NotesWindow w(*m_store, *m_settings);
        QVERIFY(w.currentId().isEmpty());
        QVERIFY(!w.editor()->isEnabled());
        QCOMPARE(w.noteCount(), 0);
    }

    void typingIsSavedAutomatically()
    {
        NotesWindow w(*m_store, *m_settings);
        const QString id = w.newNote();
        QVERIFY(!id.isEmpty());
        QVERIFY(w.editor()->isEnabled());
        w.editor()->setPlainText(QStringLiteral("Groceries\nmilk"));
        QVERIFY(w.hasUnsavedChanges());
        QTRY_VERIFY_WITH_TIMEOUT(!w.hasUnsavedChanges(), 3000);
        QCOMPARE(m_store->load(id).text, QStringLiteral("Groceries\nmilk"));
        QVERIFY(w.statusText().contains(QStringLiteral("saved")));
    }

    void flushSavesImmediately()
    {
        NotesWindow w(*m_store, *m_settings);
        const QString id = w.newNote();
        w.editor()->setPlainText(QStringLiteral("quick"));
        w.flush();
        QVERIFY(!w.hasUnsavedChanges());
        QCOMPARE(m_store->load(id).text, QStringLiteral("quick"));
    }

    void switchingNotesSavesTheOldOne()
    {
        NotesWindow w(*m_store, *m_settings);
        const QString a = w.newNote();
        w.editor()->setPlainText(QStringLiteral("alpha"));
        const QString b = w.newNote();
        QCOMPARE(m_store->load(a).text, QStringLiteral("alpha"));
        w.editor()->setPlainText(QStringLiteral("beta"));
        QVERIFY(w.openNote(a));
        QCOMPARE(m_store->load(b).text, QStringLiteral("beta"));
        QCOMPARE(w.editor()->toPlainText(), QStringLiteral("alpha"));
    }

    void emptyNotesAreDroppedWhenLeft()
    {
        NotesWindow w(*m_store, *m_settings);
        const QString a = w.newNote();
        w.editor()->setPlainText(QStringLiteral("keep"));
        w.flush();
        const QString empty = w.newNote();
        QVERIFY(m_store->exists(empty));
        QVERIFY(w.openNote(a));
        QVERIFY(!m_store->exists(empty));
        QCOMPARE(w.noteCount(), 1);
    }

    void closingSavesPendingText()
    {
        QString id;
        {
            NotesWindow w(*m_store, *m_settings);
            id = w.newNote();
            w.editor()->setPlainText(QStringLiteral("unsaved"));
            w.show();
            w.close();
        }
        QCOMPARE(m_store->load(id).text, QStringLiteral("unsaved"));
    }

    void reopensExistingNotesNewestFirst()
    {
        const QString a = m_store->create(QStringLiteral("old"));
        QFile f(m_tmp.path() + QStringLiteral("/notes/%1.txt").arg(a));
        QVERIFY(f.open(QIODevice::ReadWrite));
        QVERIFY(f.setFileTime(QDateTime::currentDateTime().addSecs(-500), QFileDevice::FileModificationTime));
        f.close();
        const QString b = m_store->create(QStringLiteral("new"));
        NotesWindow w(*m_store, *m_settings);
        QCOMPARE(w.currentId(), b);
        QCOMPARE(w.visibleIds(), (QStringList{b, a}));
        QCOMPARE(w.editor()->toPlainText(), QStringLiteral("new"));
    }

    void searchFiltersList()
    {
        m_store->create(QStringLiteral("Cats\nmeow"));
        m_store->create(QStringLiteral("Dogs\nwoof"));
        NotesWindow w(*m_store, *m_settings);
        QCOMPARE(w.visibleIds().size(), 2);
        w.setSearch(QStringLiteral("woof"));
        QCOMPARE(w.visibleIds().size(), 1);
        w.setSearch(QString());
        QCOMPARE(w.visibleIds().size(), 2);
    }

    void searchWhileEditingDoesNotLoseText()
    {
        NotesWindow w(*m_store, *m_settings);
        const QString id = w.newNote();
        w.editor()->setPlainText(QStringLiteral("typed text"));
        w.setSearch(QStringLiteral("typed"));
        QCOMPARE(m_store->load(id).text, QStringLiteral("typed text"));
        QCOMPARE(w.editor()->toPlainText(), QStringLiteral("typed text"));
    }

    void newNoteClearsSearch()
    {
        m_store->create(QStringLiteral("Cats"));
        NotesWindow w(*m_store, *m_settings);
        w.setSearch(QStringLiteral("cats"));
        const QString id = w.newNote();
        QVERIFY(w.visibleIds().contains(id));
    }

    void deleteRemovesAndSelectsNext()
    {
        const QString a = m_store->create(QStringLiteral("one"));
        QFile f(m_tmp.path() + QStringLiteral("/notes/%1.txt").arg(a));
        QVERIFY(f.open(QIODevice::ReadWrite));
        QVERIFY(f.setFileTime(QDateTime::currentDateTime().addSecs(-500), QFileDevice::FileModificationTime));
        f.close();
        const QString b = m_store->create(QStringLiteral("two"));
        NotesWindow w(*m_store, *m_settings);
        QCOMPARE(w.currentId(), b);
        QVERIFY(w.deleteCurrent());
        QVERIFY(!m_store->exists(b));
        QCOMPARE(w.currentId(), a);
        QVERIFY(w.deleteCurrent());
        QVERIFY(w.currentId().isEmpty());
        QVERIFY(!w.editor()->isEnabled());
        QVERIFY(!w.deleteCurrent());
    }

    void deletedNoteIsNotResurrectedByPendingSave()
    {
        NotesWindow w(*m_store, *m_settings);
        const QString id = w.newNote();
        w.editor()->setPlainText(QStringLiteral("doomed"));
        QVERIFY(w.deleteCurrent());
        QTest::qWait(NotesWindow::kSaveDelayMs + 300);
        QVERIFY(!m_store->exists(id));
    }

    void openingMissingNoteFails()
    {
        NotesWindow w(*m_store, *m_settings);
        QVERIFY(!w.openNote(QStringLiteral("ghost")));
    }

    void statusShowsCounts()
    {
        NotesWindow w(*m_store, *m_settings);
        w.newNote();
        w.editor()->setPlainText(QStringLiteral("one two three"));
        QVERIFY(w.statusText().contains(QStringLiteral("3")));
        QVERIFY(w.statusText().contains(QStringLiteral("13")));
    }

    void saveFailureKeepsTextAndFlagsDirty()
    {
        NotesWindow w(*m_store, *m_settings);
        const QString id = w.newNote();
        // Replace the notes folder by a file so that saving cannot work.
        QDir(m_tmp.path() + QStringLiteral("/notes")).removeRecursively();
        QFile blocker(m_tmp.path() + QStringLiteral("/notes"));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        w.editor()->setPlainText(QStringLiteral("precious"));
        w.flush();
        QVERIFY(w.hasUnsavedChanges());
        QCOMPARE(w.editor()->toPlainText(), QStringLiteral("precious"));
        QVERIFY(w.statusText().contains(QStringLiteral("not saved")));
        // The problem goes away: the text is saved on the next try.
        QVERIFY(QFile::remove(m_tmp.path() + QStringLiteral("/notes")));
        w.flush();
        QVERIFY(!w.hasUnsavedChanges());
        QCOMPARE(m_store->load(id).text, QStringLiteral("precious"));
    }

    void geometryIsRemembered()
    {
        {
            NotesWindow w(*m_store, *m_settings);
            w.resize(701, 433);
            w.show();
            w.close();
        }
        QVERIFY(!m_settings->value(Keys::NotesGeometry).toByteArray().isEmpty());
    }

private:
    QTemporaryDir m_tmp;
    std::unique_ptr<NoteStore> m_store;
    std::unique_ptr<SettingsStore> m_settings;
};

QTEST_MAIN(NotesUiTest)
#include "test_ui.moc"
