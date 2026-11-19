// Notes: plain text notes, one file each, on Raven.
#include "NotesApp.hpp"

#include <cstring>

int main(int argc, char** argv) {
	const bool headless = argc > 1 && std::strcmp(argv[1], "--headless") == 0;
	if (!colibry::init({"Notes", headless})) return 1;

	colibry::notes::NotesApp app(colibry::notes::NoteStore(colibry::notes::NoteStore::defaultDirectory()));
	raven::Window window = colibry::window_from_file(colibry::sharePath("notes/notes.xml"));
	if (!window.is_valid()) return 1;
	app.attach(window);

	while (raven::wait_frame(0.5)) app.tick();  // false once the window is closed or app.quit fired
	app.flush();
	raven::shutdown();
	return 0;
}
