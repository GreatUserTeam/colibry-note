#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

namespace colibry::notes {

struct NoteInfo
{
    QString id;
    QString title;
    QString preview;
    QDateTime modified;
};

/// Notes kept as plain UTF-8 text files, one per note, inside one folder.
///
/// Plain files mean that a damaged note never affects the others, and that a person can open,
/// copy or back up the notes with any other program. Writes are atomic: the new text is written
/// to a temporary file and then renamed over the old one, so a crash or a full disk cannot leave
/// a half-written note behind.
class NoteStore
{
public:
    struct Loaded
    {
        bool ok = false;
        QString text;
        QString error;
    };
    struct Result
    {
        bool ok = false;
        QString error;
    };

    explicit NoteStore(QString directory);

    static QString defaultDirectory();
    QString directory() const { return m_dir; }

    /// Ids are used as file names, so only letters, digits, "-" and "_" are accepted.
    /// This also makes "../../etc/passwd" impossible.
    static bool isValidId(const QString &id);

    /// Makes a new note (an empty file unless text is given) and returns its id; empty on failure.
    QString create(const QString &text = {});
    bool exists(const QString &id) const;
    Loaded load(const QString &id) const;
    Result save(const QString &id, const QString &text);
    bool remove(const QString &id);

    /// Newest first. A non-empty query keeps only notes whose text contains it (ignoring case).
    QList<NoteInfo> list(const QString &query = {}) const;

    /// First non-empty line, shortened; "Untitled note" for an empty text.
    static QString titleOf(const QString &text);
    /// The text after the title, on one line, shortened.
    static QString previewOf(const QString &text);
    static int wordCount(const QString &text);

private:
    QString pathFor(const QString &id) const;

    QString m_dir;
};

} // namespace colibry::notes
