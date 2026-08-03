#pragma once

#include "ICommand.h"
#include <memory>
#include <vector>
#include <utility>

namespace FufuStudio
{
	// Editor undo/redo stack. Every scene mutation made from
	// the panels must go through here rather than calling Scene directly,
	// otherwise it becomes invisible to Ctrl+Z.
	class CommandHistory
	{
	public:
		void execute(std::unique_ptr<ICommand> command)
		{
			command->execute();
			m_UndoStack.push_back(std::move(command));
			m_RedoStack.clear();
		}

		// Constructs the command, executes it, and returns a raw pointer so
		// the caller can read its result (e.g. the created entity).
		// The object remains alive as long as it has not been popped from the history.
		template<typename T, typename... Args>
		T* executeCommand(Args&&... args)
		{
			auto command = std::make_unique<T>(std::forward<Args>(args)...);
			T* raw = command.get();
			execute(std::move(command));
			return raw;
		}

		void undo()
		{
			if (m_UndoStack.empty()) return;

			std::unique_ptr<ICommand> command = std::move(m_UndoStack.back());
			m_UndoStack.pop_back();
			command->undo();
			m_RedoStack.push_back(std::move(command));
		}

		void redo()
		{
			if (m_RedoStack.empty()) return;

			std::unique_ptr<ICommand> command = std::move(m_RedoStack.back());
			m_RedoStack.pop_back();
			command->redo();
			m_UndoStack.push_back(std::move(command));
		}

		bool canUndo() const { return !m_UndoStack.empty(); }
		bool canRedo() const { return !m_RedoStack.empty(); }

		const char* peekUndoName() const { return m_UndoStack.empty() ? "" : m_UndoStack.back()->getName(); }
		const char* peekRedoName() const { return m_RedoStack.empty() ? "" : m_RedoStack.back()->getName(); }

		// Call when the active scene changes (new scene, load):
		// the history references entities from the old scene and becomes invalid.
		void clear()
		{
			m_UndoStack.clear();
			m_RedoStack.clear();
		}

	private:
		std::vector<std::unique_ptr<ICommand>> m_UndoStack;
		std::vector<std::unique_ptr<ICommand>> m_RedoStack;
	};
}
