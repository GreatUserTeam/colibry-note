#include "NotesWindow.h"
#include "colibry/SettingsStore.h"
#include "colibry/ThemeSync.h"

#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("colibry-notes"));
    QApplication::setApplicationDisplayName(QStringLiteral("Notes"));
    QApplication::setApplicationVersion(QStringLiteral(Colibry_APPS_VERSION));
    QApplication::setOrganizationName(QStringLiteral("GreatUserTeam"));
    QApplication::setDesktopFileName(QStringLiteral("colibry-notes"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Notes for the Great User System"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    colibry::SettingsStore settings;
    colibry::ThemeSync themeSync(app, settings);

    colibry::notes::NoteStore notes(colibry::notes::NoteStore::defaultDirectory());
    colibry::notes::NotesWindow window(notes, settings);
    window.show();
    return QApplication::exec();
}
