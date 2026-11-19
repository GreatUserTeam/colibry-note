// Notes kept as plain UTF-8 text files, one per note, in one folder (MIT).
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace colibry::notes {

struct NoteInfo {
	std::string id;
	std::string title;
	std::string preview;
	std::filesystem::file_time_type modified;
};

class NoteStore {
public:
	explicit NoteStore(std::filesystem::path directory);

	// $COLIBRY_NOTES_DIR, else $XDG_DATA_HOME/colibry/notes, else ~/.local/share/colibry/notes.
	static std::filesystem::path defaultDirectory();
	const std::filesystem::path& directory() const { return m_dir; }

	// Ids become file names: only letters, digits, '-' and '_', 1 to 64 long.
	static bool isValidId(const std::string& id);

	// A new note (empty unless text is given). Returns its id, or "" on failure.
	std::string create(const std::string& text = {});
	bool exists(const std::string& id) const;
	std::optional<std::string> load(const std::string& id, std::string* error = nullptr) const;
	// Atomic: the text goes to "<id>.txt.tmp" first and is renamed over the note.
	bool save(const std::string& id, const std::string& text, std::string* error = nullptr);
	bool remove(const std::string& id);

	// Newest first. A non-empty query keeps only notes containing it (case-insensitive, Latin and Cyrillic).
	std::vector<NoteInfo> list(const std::string& query = {}) const;

	static std::string titleOf(const std::string& text);    // first non-empty line, shortened
	static std::string previewOf(const std::string& text);  // the lines after the title, one line
	static int wordCount(const std::string& text);
	static bool containsIgnoreCase(const std::string& text, const std::string& needle);

private:
	std::filesystem::path pathFor(const std::string& id) const;

	std::filesystem::path m_dir;
};

}  // namespace colibry::notes
