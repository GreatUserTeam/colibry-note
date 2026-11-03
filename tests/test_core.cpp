#include "core/NoteStore.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace colibry::notes;

class NoteStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        QVERIFY(m_tmp.isValid());
        m_dir = m_tmp.path() + QStringLiteral("/notes");
        QDir(m_dir).removeRecursively();
    }

    void createsAndLoads()
    {
        NoteStore s(m_dir);
        const QString id = s.create(QStringLiteral("Hello\nworld"));
        QVERIFY(!id.isEmpty());
        QVERIFY(s.exists(id));
        const auto l = s.load(id);
        QVERIFY(l.ok);
        QCOMPARE(l.text, QStringLiteral("Hello\nworld"));
    }

    void idsAreUnique()
    {
        NoteStore s(m_dir);
        QSet<QString> ids;
        for (int i = 0; i < 50; ++i)
            ids.insert(s.create());
        QCOMPARE(ids.size(), 50);
    }

    void rejectsPathTraversalIds()
    {
        NoteStore s(m_dir);
        const QStringList bad = {QStringLiteral("../x"), QStringLiteral("a/b"), QString(), QStringLiteral("a.b"),
                                 QStringLiteral("/etc/passwd"), QString(70, QLatin1Char('a'))};
        for (const QString &id : bad) {
            QVERIFY2(!NoteStore::isValidId(id), qPrintable(id));
            QVERIFY(!s.load(id).ok);
            QVERIFY(!s.save(id, QStringLiteral("x")).ok);
            QVERIFY(!s.remove(id));
            QVERIFY(!s.exists(id));
        }
        QVERIFY(!QFileInfo::exists(m_tmp.path() + QStringLiteral("/x.txt")));
    }

    void saveReplacesTextAndKeepsUnicode()
    {
        NoteStore s(m_dir);
        const QString id = s.create(QStringLiteral("old"));
        const QString text = QStringLiteral("Привет, мир! 日本語 😀");
        QVERIFY(s.save(id, text).ok);
        QCOMPARE(s.load(id).text, text);
    }

    void saveLeavesNoTemporaryFiles()
    {
        NoteStore s(m_dir);
        const QString id = s.create(QStringLiteral("a"));
        for (int i = 0; i < 5; ++i)
            QVERIFY(s.save(id, QString::number(i)).ok);
        QCOMPARE(QDir(m_dir).entryList(QDir::Files).size(), 1);
    }

    void removeDeletesOnlyThatNote()
    {
        NoteStore s(m_dir);
        const QString a = s.create(QStringLiteral("a"));
        const QString b = s.create(QStringLiteral("b"));
        QVERIFY(s.remove(a));
        QVERIFY(!s.exists(a));
        QVERIFY(s.exists(b));
        QVERIFY(!s.remove(a));
    }

    void loadMissingFails()
    {
        NoteStore s(m_dir);
        const auto l = s.load(QStringLiteral("nope"));
        QVERIFY(!l.ok);
        QVERIFY(!l.error.isEmpty());
    }

    void readsBomAndLatin1Files()
    {
        NoteStore s(m_dir);
        QDir().mkpath(m_dir);
        {
            QFile f(m_dir + QStringLiteral("/bom.txt"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("\xEF\xBB\xBF" "text");
        }
        {
            QFile f(m_dir + QStringLiteral("/latin.txt"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("caf\xE9");
        }
        QCOMPARE(s.load(QStringLiteral("bom")).text, QStringLiteral("text"));
        QCOMPARE(s.load(QStringLiteral("latin")).text, QStringLiteral("café"));
    }

    void listIsNewestFirstAndIgnoresStrangeFiles()
    {
        NoteStore s(m_dir);
        const QString a = s.create(QStringLiteral("first"));
        const QString b = s.create(QStringLiteral("second"));
        QFile::setPermissions(m_dir + QStringLiteral("/%1.txt").arg(a), QFile::ReadOwner | QFile::WriteOwner);
        // make timestamps clearly different
        QFile fa(m_dir + QStringLiteral("/%1.txt").arg(a));
        QVERIFY(fa.open(QIODevice::ReadWrite));
        QVERIFY(fa.setFileTime(QDateTime::currentDateTime().addSecs(-100), QFileDevice::FileModificationTime));
        fa.close();
        {
            QFile junk(m_dir + QStringLiteral("/readme.md"));
            QVERIFY(junk.open(QIODevice::WriteOnly));
            junk.write("x");
            QFile bad(m_dir + QStringLiteral("/bad name.txt"));
            QVERIFY(bad.open(QIODevice::WriteOnly));
            bad.write("x");
        }
        QDir().mkpath(m_dir + QStringLiteral("/dir.txt")); // a folder named like a note
        const auto list = s.list();
        QCOMPARE(list.size(), 2);
        QCOMPARE(list.at(0).id, b);
        QCOMPARE(list.at(1).id, a);
    }

    void searchIgnoresCase()
    {
        NoteStore s(m_dir);
        s.create(QStringLiteral("Shopping\nMilk and eggs"));
        s.create(QStringLiteral("Work\nCall Bob"));
        QCOMPARE(s.list(QStringLiteral("MILK")).size(), 1);
        QCOMPARE(s.list(QStringLiteral("zzz")).size(), 0);
        QCOMPARE(s.list(QStringLiteral("  ")).size(), 2);
        QCOMPARE(s.list(QStringLiteral("мол")).size(), 0);
    }

    void searchWorksForCyrillic()
    {
        NoteStore s(m_dir);
        s.create(QStringLiteral("Покупки\nМолоко"));
        QCOMPARE(s.list(QStringLiteral("молоко")).size(), 1);
    }

    void titleAndPreview()
    {
        QCOMPARE(NoteStore::titleOf(QStringLiteral("\n\n  Title  \nbody")), QStringLiteral("Title"));
        QCOMPARE(NoteStore::previewOf(QStringLiteral("\n\n  Title  \nbody\nmore")), QStringLiteral("body more"));
        QCOMPARE(NoteStore::previewOf(QStringLiteral("only title")), QString());
        QCOMPARE(NoteStore::titleOf(QString()), QStringLiteral("Untitled note"));
        QCOMPARE(NoteStore::titleOf(QStringLiteral("  \n \n")), QStringLiteral("Untitled note"));
        const QString longTitle = NoteStore::titleOf(QString(500, QLatin1Char('x')));
        QVERIFY(longTitle.size() <= 60);
        QVERIFY(longTitle.endsWith(QChar(0x2026)));
        QVERIFY(NoteStore::previewOf(QStringLiteral("t\n") + QString(500, QLatin1Char('y'))).size() <= 90);
    }

    void wordCount()
    {
        QCOMPARE(NoteStore::wordCount(QString()), 0);
        QCOMPARE(NoteStore::wordCount(QStringLiteral("  \n\t ")), 0);
        QCOMPARE(NoteStore::wordCount(QStringLiteral("one two\nthree\t four")), 4);
        QCOMPARE(NoteStore::wordCount(QStringLiteral("привет мир")), 2);
    }

    void failsCleanlyWhenFolderCannotBeCreated()
    {
        QFile blocker(m_tmp.path() + QStringLiteral("/file"));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        NoteStore s(m_tmp.path() + QStringLiteral("/file/sub"));
        QVERIFY(s.create(QStringLiteral("x")).isEmpty());
        QVERIFY(!s.save(QStringLiteral("abc"), QStringLiteral("x")).ok);
        QVERIFY(s.list().isEmpty());
    }

    void listOfMissingFolderIsEmpty()
    {
        NoteStore s(m_tmp.path() + QStringLiteral("/never-created"));
        QVERIFY(s.list().isEmpty());
    }

private:
    QTemporaryDir m_tmp;
    QString m_dir;
};

QTEST_MAIN(NoteStoreTest)
#include "test_core.moc"
