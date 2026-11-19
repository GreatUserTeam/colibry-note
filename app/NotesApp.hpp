// The Notes window logic on Raven (MIT). Keep the object alive while the window is open:
// Raven callbacks point back into it.
#pragma once

#include "NoteStore.hpp"

#include <colibry/app.hpp>

#include <string>
#include <vector>

namespace colibry::notes {

class NotesApp {
public:
	explicit NotesApp(NoteStore store);

	// Connects the widgets of notes.xml (search, list, editor, status) and the actions
	// notes.new, notes.delete. Call once, after the window was created from the XML.
	void attach(raven::Window window);

	void newNote();
	void deleteCurrent();
	void setQuery(const std::string& query);
	void selectRow(int row);
	// Saves the open note when its text changed. Called on every tick and before quitting.
	void flush();
	// Periodic work for the main loop.
	void tick() { flush(); }

	const std::string& currentId() const { return m_current; }
	std::size_t visibleCount() const { return m_shown.size(); }
	const NoteStore& store() const { return m_store; }

private:
	void refreshList();
	void showStatus(const std::string& message);

	NoteStore m_store;
	std::string m_query;
	std::string m_current;  // id of the open note, empty when none
	std::string m_saved;    // text as it is on disk
	std::vector<NoteInfo> m_shown;

	raven::Window m_window;
	raven::TextInput m_search;
	raven::ListView m_list;
	raven::TextEditor m_editor;
	raven::Label m_status;
};

}  // namespace colibry::notes
