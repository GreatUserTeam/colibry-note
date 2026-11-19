#include "NoteStore.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <system_error>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace colibry::notes {
namespace {

constexpr std::size_t kTitleLength = 60;
constexpr std::size_t kPreviewLength = 90;
constexpr const char* kEllipsis = "\xE2\x80\xA6";  // U+2026

bool isLead(char c) { return (static_cast<unsigned char>(c) & 0xC0) != 0x80; }

std::size_t codePoints(const std::string& s) {
	return static_cast<std::size_t>(std::count_if(s.begin(), s.end(), isLead));
}

bool validUtf8(const std::string& s) {
	std::size_t i = 0;
	while (i < s.size()) {
		const auto c = static_cast<unsigned char>(s[i]);
		std::size_t n = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 0;
		if (n == 0 || i + n > s.size()) return false;
		for (std::size_t k = 1; k < n; ++k)
			if ((static_cast<unsigned char>(s[i + k]) >> 6) != 0x2) return false;
		i += n;
	}
	return true;
}

// Files written by other programs in an 8-bit encoding: each byte becomes one code point.
std::string latin1ToUtf8(const std::string& s) {
	std::string out;
	for (unsigned char c : s) {
		if (c < 0x80) {
			out += static_cast<char>(c);
		} else {
			out += static_cast<char>(0xC0 | (c >> 6));
			out += static_cast<char>(0x80 | (c & 0x3F));
		}
	}
	return out;
}

std::string normalizeText(std::string bytes) {
	if (bytes.size() >= 3 && bytes.compare(0, 3, "\xEF\xBB\xBF") == 0) bytes.erase(0, 3);  // BOM
	if (!validUtf8(bytes)) bytes = latin1ToUtf8(bytes);
	std::string out;
	out.reserve(bytes.size());
	for (std::size_t i = 0; i < bytes.size(); ++i) {
		if (bytes[i] == '\r' && i + 1 < bytes.size() && bytes[i + 1] == '\n') continue;
		out += bytes[i];
	}
	return out;
}

bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

// Collapses runs of whitespace and trims both ends.
std::string simplified(const std::string& s) {
	std::string out;
	bool pendingSpace = false;
	for (char c : s) {
		if (isSpace(c)) {
			pendingSpace = !out.empty();
		} else {
			if (pendingSpace) out += ' ';
			pendingSpace = false;
			out += c;
		}
	}
	return out;
}

std::string shorten(const std::string& text, std::size_t length) {
	std::string s = simplified(text);
	if (codePoints(s) <= length) return s;
	std::size_t count = 0, end = s.size();
	for (std::size_t i = 0; i < s.size(); ++i) {
		if (isLead(s[i])) {
			if (count == length - 1) {
				end = i;
				break;
			}
			++count;
		}
	}
	std::string out = s.substr(0, end);
	while (!out.empty() && out.back() == ' ') out.pop_back();
	return out + kEllipsis;
}

// Lower-cases ASCII, basic Cyrillic (U+0410-U+042F, U+0400-U+040F) and Latin-1 letters, keeping UTF-8.
std::string foldCase(const std::string& s) {
	std::string out;
	out.reserve(s.size());
	for (std::size_t i = 0; i < s.size();) {
		const auto c = static_cast<unsigned char>(s[i]);
		if (c < 0x80) {
			out += (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : static_cast<char>(c);
			++i;
			continue;
		}
		if ((c >> 5) == 0x6 && i + 1 < s.size()) {
			unsigned cp = ((c & 0x1Fu) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3Fu);
			if (cp >= 0x410 && cp <= 0x42F) cp += 0x20;
			else if (cp >= 0x400 && cp <= 0x40F) cp += 0x50;
			out += static_cast<char>(0xC0 | (cp >> 6));
			out += static_cast<char>(0x80 | (cp & 0x3F));
			i += 2;
			continue;
		}
		std::size_t n = (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 1;
		out += s.substr(i, n);
		i += n;
	}
	return out;
}

std::string timestampId() {
	using namespace std::chrono;
	const auto now = system_clock::now();
	const std::time_t secs = system_clock::to_time_t(now);
	const auto ms = duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000;
	std::tm utc{};
#ifdef _WIN32
	gmtime_s(&utc, &secs);
#else
	gmtime_r(&secs, &utc);
#endif
	std::ostringstream id;
	id << std::put_time(&utc, "%Y%m%d-%H%M%S") << '-' << std::setw(3) << std::setfill('0') << ms << '-'
	   << std::hex << std::setw(4) << std::setfill('0') << std::random_device{}() % 0x10000;
	return id.str();
}

}  // namespace

NoteStore::NoteStore(fs::path directory) : m_dir(std::move(directory)) {}

fs::path NoteStore::defaultDirectory() {
	if (const char* dir = std::getenv("COLIBRY_NOTES_DIR"); dir && *dir) return dir;
	if (const char* data = std::getenv("XDG_DATA_HOME"); data && *data) return fs::path(data) / "colibry" / "notes";
	const char* home = std::getenv("HOME");
	return fs::path(home ? home : ".") / ".local" / "share" / "colibry" / "notes";
}

bool NoteStore::isValidId(const std::string& id) {
	if (id.empty() || id.size() > 64) return false;
	return std::all_of(id.begin(), id.end(), [](char c) {
		return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
	});
}

fs::path NoteStore::pathFor(const std::string& id) const { return m_dir / (id + ".txt"); }

bool NoteStore::exists(const std::string& id) const {
	std::error_code ec;
	return isValidId(id) && fs::is_regular_file(pathFor(id), ec);
}

std::string NoteStore::create(const std::string& text) {
	for (int attempt = 0; attempt < 20; ++attempt) {
		const std::string id = timestampId();
		if (exists(id)) continue;
		if (!save(id, text)) return {};
		return id;
	}
	return {};
}

std::optional<std::string> NoteStore::load(const std::string& id, std::string* error) const {
	if (!isValidId(id)) {
		if (error) *error = "Invalid note.";
		return std::nullopt;
	}
	std::ifstream in(pathFor(id), std::ios::binary);
	if (!in) {
		if (error) *error = "Cannot open the note.";
		return std::nullopt;
	}
	std::stringstream buffer;
	buffer << in.rdbuf();
	return normalizeText(buffer.str());
}

bool NoteStore::save(const std::string& id, const std::string& text, std::string* error) {
	if (!isValidId(id)) {
		if (error) *error = "Invalid note.";
		return false;
	}
	std::error_code ec;
	fs::create_directories(m_dir, ec);
	if (ec) {
		if (error) *error = "Cannot create the notes folder.";
		return false;
	}
	const fs::path target = pathFor(id);
	const fs::path temp = target.string() + ".tmp";
	{
		std::ofstream out(temp, std::ios::binary | std::ios::trunc);
		out << text;
		out.flush();
		if (!out) {
			if (error) *error = "Cannot write the note.";
			fs::remove(temp, ec);
			return false;
		}
	}
	fs::rename(temp, target, ec);
	if (ec) {  // some platforms refuse to rename over an existing file
		fs::remove(target, ec);
		fs::rename(temp, target, ec);
	}
	if (ec) {
		if (error) *error = "Cannot save the note.";
		return false;
	}
	return true;
}

bool NoteStore::remove(const std::string& id) {
	std::error_code ec;
	return isValidId(id) && fs::remove(pathFor(id), ec) && !ec;
}

std::string NoteStore::titleOf(const std::string& text) {
	std::istringstream lines(text);
	std::string line;
	while (std::getline(lines, line)) {
		const std::string trimmed = simplified(line);
		if (!trimmed.empty()) return shorten(trimmed, kTitleLength);
	}
	return "Untitled note";
}

std::string NoteStore::previewOf(const std::string& text) {
	std::istringstream lines(text);
	std::string line, rest;
	bool titleSeen = false;
	while (std::getline(lines, line)) {
		if (!titleSeen) {
			if (!simplified(line).empty()) titleSeen = true;
			continue;
		}
		rest += line + ' ';
	}
	return shorten(rest, kPreviewLength);
}

int NoteStore::wordCount(const std::string& text) {
	int count = 0;
	bool inWord = false;
	for (char c : text) {
		if (isSpace(c)) {
			inWord = false;
		} else if (!inWord) {
			inWord = true;
			++count;
		}
	}
	return count;
}

bool NoteStore::containsIgnoreCase(const std::string& text, const std::string& needle) {
	const std::string n = foldCase(simplified(needle));
	return n.empty() || foldCase(text).find(n) != std::string::npos;
}

std::vector<NoteInfo> NoteStore::list(const std::string& query) const {
	std::vector<NoteInfo> notes;
	std::error_code ec;
	for (fs::directory_iterator it(m_dir, ec), end; !ec && it != end; it.increment(ec)) {
		const fs::path path = it->path();
		if (path.extension() != ".txt" || !it->is_regular_file(ec)) continue;
		const std::string id = path.stem().string();
		if (!isValidId(id)) continue;
		auto text = load(id);
		if (!text) continue;
		if (!query.empty() && !containsIgnoreCase(*text, query)) continue;
		notes.push_back({id, titleOf(*text), previewOf(*text), fs::last_write_time(path, ec)});
	}
	std::sort(notes.begin(), notes.end(), [](const NoteInfo& a, const NoteInfo& b) {
		return a.modified != b.modified ? a.modified > b.modified : a.id > b.id;
	});
	return notes;
}

}  // namespace colibry::notes
