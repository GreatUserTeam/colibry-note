#include "NoteStore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringDecoder>

#include <algorithm>

namespace colibry::notes {

namespace {

constexpr int kTitleLength = 60;
constexpr int kPreviewLength = 90;

QString decode(const QByteArray &bytes)
{
    QStringDecoder utf8(QStringConverter::Utf8, QStringConverter::Flag::Stateless);
    QString text = utf8(bytes);
    if (utf8.hasError())
        text = QString::fromLatin1(bytes); // written by another program in an 8-bit encoding
    if (text.startsWith(QChar(0xFEFF)))
        text.remove(0, 1);
    return text;
}

QString shorten(QString text, int length)
{
    text = text.simplified();
    if (text.size() > length)
        text = text.left(length - 1).trimmed() + QChar(0x2026);
    return text;
}

} // namespace

NoteStore::NoteStore(QString directory)
    : m_dir(std::move(directory))
{
}

QString NoteStore::defaultDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/notes");
}

bool NoteStore::isValidId(const QString &id)
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9_-]{1,64}$"));
    return pattern.match(id).hasMatch();
}

QString NoteStore::pathFor(const QString &id) const
{
    return m_dir + QLatin1Char('/') + id + QStringLiteral(".txt");
}

bool NoteStore::exists(const QString &id) const
{
    return isValidId(id) && QFileInfo(pathFor(id)).isFile();
}

QString NoteStore::create(const QString &text)
{
    if (!QDir().mkpath(m_dir))
        return {};

    for (int attempt = 0; attempt < 20; ++attempt) {
        const QString id = QStringLiteral("%1-%2")
                               .arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz")))
                               .arg(QRandomGenerator::global()->bounded(0x10000), 4, 16, QLatin1Char('0'));
        if (exists(id))
            continue;
        if (!save(id, text).ok)
            return {};
        return id;
    }
    return {};
}

NoteStore::Loaded NoteStore::load(const QString &id) const
{
    Loaded result;
    if (!isValidId(id)) {
        result.error = QCoreApplication::translate("colibry::notes", "Invalid note.");
        return result;
    }
    QFile file(pathFor(id));
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = file.errorString();
        return result;
    }
    result.text = decode(file.readAll());
    result.ok = true;
    return result;
}

NoteStore::Result NoteStore::save(const QString &id, const QString &text)
{
    if (!isValidId(id))
        return {false, QCoreApplication::translate("colibry::notes", "Invalid note.")};
    if (!QDir().mkpath(m_dir))
        return {false, QCoreApplication::translate("colibry::notes", "Cannot create the notes folder.")};

    QSaveFile file(pathFor(id));
    if (!file.open(QIODevice::WriteOnly))
        return {false, file.errorString()};
    file.write(text.toUtf8());
    if (!file.commit())
        return {false, file.errorString()};
    return {true, {}};
}

bool NoteStore::remove(const QString &id)
{
    return isValidId(id) && QFile::remove(pathFor(id));
}

QString NoteStore::titleOf(const QString &text)
{
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty())
            return shorten(trimmed, kTitleLength);
    }
    return QCoreApplication::translate("colibry::notes", "Untitled note");
}

QString NoteStore::previewOf(const QString &text)
{
    const QStringList lines = text.split(QLatin1Char('\n'));
    int i = 0;
    while (i < lines.size() && lines.at(i).trimmed().isEmpty())
        ++i;
    ++i; // skip the title line
    QStringList rest;
    for (; i < lines.size(); ++i)
        rest << lines.at(i);
    return shorten(rest.join(QLatin1Char(' ')), kPreviewLength);
}

int NoteStore::wordCount(const QString &text)
{
    static const QRegularExpression word(QStringLiteral("\\S+"));
    int count = 0;
    auto it = word.globalMatch(text);
    while (it.hasNext()) {
        it.next();
        ++count;
    }
    return count;
}

QList<NoteInfo> NoteStore::list(const QString &query) const
{
    QList<NoteInfo> notes;
    const QFileInfoList files = QDir(m_dir).entryInfoList({QStringLiteral("*.txt")}, QDir::Files | QDir::Readable);
    const QString needle = query.trimmed();

    for (const QFileInfo &info : files) {
        const QString id = info.completeBaseName();
        if (!isValidId(id))
            continue;
        const Loaded loaded = load(id);
        if (!loaded.ok)
            continue;
        if (!needle.isEmpty() && !loaded.text.contains(needle, Qt::CaseInsensitive))
            continue;
        notes.append({id, titleOf(loaded.text), previewOf(loaded.text), info.lastModified()});
    }

    std::sort(notes.begin(), notes.end(), [](const NoteInfo &a, const NoteInfo &b) {
        return a.modified != b.modified ? a.modified > b.modified : a.id > b.id;
    });
    return notes;
}

} // namespace colibry::notes
