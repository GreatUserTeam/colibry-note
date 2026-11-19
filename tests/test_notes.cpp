// Notes tests: the store (no window) and the window logic (headless, Raven's null backend).
#include "../app/NotesApp.hpp"

#include <colibry/app.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;
using colibry::notes::NoteStore;
using colibry::notes::NotesApp;

#define EXPECT(c) do { if (!(c)) { std::printf("FAILED %s:%d: %s\n", __FILE__, __LINE__, #c); return 1; } } while (0)

#ifndef COLIBRY_SOURCE_DIR
#define COLIBRY_SOURCE_DIR "."
#endif

static std::string readFile(const fs::path& p) {
	std::ifstream in(p, std::ios::binary);
	std::stringstream s;
	s << in.rdbuf();
	return s.str();
}

static int testText() {
	EXPECT(NoteStore::titleOf("\n  \n Покупки  \nмолоко") == "Покупки");
	EXPECT(NoteStore::titleOf("   \n") == "Untitled note");
	EXPECT(NoteStore::previewOf("Title\nfirst  line\n\nsecond") == "first line second");
	EXPECT(NoteStore::previewOf("only title") == "");
	EXPECT(NoteStore::wordCount("one  two\tthree\nfour") == 4);
	EXPECT(NoteStore::wordCount("") == 0);
	// shortened titles keep whole UTF-8 characters and end with an ellipsis
	const std::string longTitle(200, 'a');
	const std::string shortened = NoteStore::titleOf(longTitle);
	EXPECT(shortened.size() == 60 - 1 + 3 && shortened.compare(shortened.size() - 3, 3, "\xE2\x80\xA6") == 0);
	std::string cyr;
	for (int i = 0; i < 100; ++i) cyr += "я";
	EXPECT(NoteStore::titleOf(cyr).size() == 59 * 2 + 3);
	// case-insensitive search for Latin and Cyrillic
	EXPECT(NoteStore::containsIgnoreCase("Привет МИР", "привет"));
	EXPECT(NoteStore::containsIgnoreCase("Hello World", "WORLD"));
	EXPECT(!NoteStore::containsIgnoreCase("Hello", "xyz"));
	EXPECT(NoteStore::containsIgnoreCase("anything", ""));
	return 0;
}

static int testIds() {
	EXPECT(NoteStore::isValidId("20260101-120000-001-abcd"));
	EXPECT(!NoteStore::isValidId("../etc/passwd"));
	EXPECT(!NoteStore::isValidId(""));
	EXPECT(!NoteStore::isValidId(std::string(65, 'a')));
	return 0;
}

static int testStore(const fs::path& dir) {
	NoteStore store(dir);
	std::string id = store.create("Hello\nworld");
	EXPECT(!id.empty() && store.exists(id));
	auto loaded = store.load(id);
	EXPECT(loaded && *loaded == "Hello\nworld");
	EXPECT(store.save(id, "Changed\r\nline") );
	EXPECT(*store.load(id) == "Changed\nline");  // CRLF is normalised on load
	EXPECT(!store.save("../bad", "x"));
	EXPECT(!fs::exists(dir / "temp.tmp"));

	std::string id2 = store.create("Привет МИР");
	EXPECT(id2 != id);
	auto all = store.list();
	EXPECT(all.size() == 2);
	auto found = store.list("привет");
	EXPECT(found.size() == 1 && found[0].id == id2);
	EXPECT(store.list("nothing-here").empty());

	EXPECT(store.remove(id));
	EXPECT(!store.exists(id) && !store.remove(id));
	EXPECT(store.list().size() == 1);

	// a file written in another encoding is still readable
	std::ofstream(dir / "legacy.txt", std::ios::binary) << "caf\xE9";
	EXPECT(*store.load("legacy") == "caf\xC3\xA9");
	return 0;
}

static int testWindow(const fs::path& dir) {
	NoteStore store(dir);
	NotesApp app(store);
	raven::Window window = colibry::window_from_file(std::string(COLIBRY_SOURCE_DIR) + "/app/notes.xml");
	EXPECT(window.is_valid());
	app.attach(window);
	const std::size_t before = app.visibleCount();

	app.newNote();
	EXPECT(!app.currentId().empty());
	EXPECT(app.visibleCount() == before + 1);
	const std::string id = app.currentId();

	raven::Widget root(window.get_content());
	raven::TextEditor editor(root.find("editor"));
	editor.set_text("Список\nкупить хлеб");
	app.flush();  // what the loop does on every tick
	EXPECT(*store.load(id) == "Список\nкупить хлеб");

	// search narrows the list; the open note stays open
	app.setQuery("ХЛЕБ");
	EXPECT(app.visibleCount() == 1);
	EXPECT(app.currentId() == id);
	app.setQuery("");

	app.newNote();
	const std::string second = app.currentId();
	EXPECT(second != id);
	// rows follow store.list(), newest first; find this note's row whatever its position
	const auto rows = store.list();
	int row = -1;
	for (std::size_t i = 0; i < rows.size(); ++i)
		if (rows[i].id == id) row = static_cast<int>(i);
	EXPECT(row >= 0);
	app.selectRow(row);
	EXPECT(app.currentId() == id);
	app.deleteCurrent();
	EXPECT(app.currentId().empty());
	EXPECT(!store.exists(id) && store.exists(second));
	return 0;
}

int main() {
	const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
	const fs::path dir = fs::temp_directory_path() / ("colibry-notes-test-" + std::to_string(stamp));
	struct Cleanup { fs::path p; ~Cleanup() { std::error_code ec; fs::remove_all(p, ec); } } cleanup{dir};
	if (testText() || testIds()) return 1;
	if (!colibry::init({"notes-test", true})) return 1;
	if (testStore(dir)) return 1;
	if (testWindow(dir)) return 1;
	raven::shutdown();
	std::printf("notes: all checks passed\n");
	return 0;
}
