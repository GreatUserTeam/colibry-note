#include "NotesApp.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace colibry::notes {

NotesApp::NotesApp(NoteStore store) : m_store(std::move(store)) {}

void NotesApp::attach(raven::Window window) {
	m_window = window;
	raven::Widget root(window.get_content());
	m_search = raven::TextInput(root.find("search"));
	m_list = raven::ListView(root.find("list"));
	m_editor = raven::TextEditor(root.find("editor"));
	m_status = raven::Label(root.find("status"));

	m_search.set_changed_callback([this](Rid, const char* text) { setQuery(text ? text : ""); });
	m_list.set_selection_changed_callback([this](Rid, int row) { selectRow(row); });
	raven::action_bind("notes.new", [this](Rid, const char*) { newNote(); });
	raven::action_bind("notes.delete", [this](Rid, const char*) { deleteCurrent(); });

	refreshList();
	if (!m_shown.empty()) selectRow(0);
	else showStatus("No notes yet. Press New note.");
}

void NotesApp::newNote() {
	flush();
	const std::string id = m_store.create();
	if (id.empty()) {
		showStatus("Cannot create a note.");
		return;
	}
	m_query.clear();
	m_search.set_text("");
	m_current = id;
	m_saved.clear();
	m_editor.set_text("");
	refreshList();
	showStatus("New note");
}

void NotesApp::deleteCurrent() {
	if (m_current.empty()) return;
	const std::string id = m_current;
	m_current.clear();
	m_saved.clear();
	m_editor.set_text("");
	if (!m_store.remove(id)) showStatus("Cannot delete the note.");
	else showStatus("Note deleted");
	refreshList();
}

void NotesApp::setQuery(const std::string& query) {
	m_query = query;
	refreshList();
}

void NotesApp::selectRow(int row) {
	if (row < 0 || row >= static_cast<int>(m_shown.size())) return;
	const std::string id = m_shown[static_cast<std::size_t>(row)].id;
	if (id == m_current) return;
	flush();
	std::string error;
	auto text = m_store.load(id, &error);
	if (!text) {
		showStatus(error);
		return;
	}
	m_current = id;
	m_saved = *text;
	m_editor.set_text(text->c_str());
	showStatus(std::to_string(NoteStore::wordCount(*text)) + " words");
}

void NotesApp::flush() {
	if (m_current.empty()) return;
	const char* raw = m_editor.get_text();
	std::string text = raw ? raw : "";
	if (text == m_saved) return;
	std::string error;
	if (!m_store.save(m_current, text, &error)) {
		showStatus(error);
		return;
	}
	m_saved = text;
	const auto it = std::find_if(m_shown.begin(), m_shown.end(), [this](const NoteInfo& n) { return n.id == m_current; });
	if (it != m_shown.end()) {
		it->title = NoteStore::titleOf(text);
		it->preview = NoteStore::previewOf(text);
		const auto row = static_cast<int>(it - m_shown.begin());
		m_list.set_item_text(row, it->title.c_str());
	}
	showStatus("Saved");
}

void NotesApp::refreshList() {
	m_shown = m_store.list(m_query);
	m_list.clear_items();
	int selected = -1;
	for (std::size_t i = 0; i < m_shown.size(); ++i) {
		m_list.add_item(m_shown[i].title.c_str());
		if (m_shown[i].id == m_current) selected = static_cast<int>(i);
	}
	m_list.set_selected(selected);
	showStatus(std::to_string(m_shown.size()) + (m_shown.size() == 1 ? " note" : " notes"));
}

void NotesApp::showStatus(const std::string& message) { m_status.set_text(message.c_str()); }

}  // namespace colibry::notes
