#pragma once

#include "ICommand.h"
#include <memory>
#include <vector>
#include <utility>

namespace FufuStudio
{
	// Pile undo/redo de l'�diteur. Toute mutation de la sc�ne faite depuis
	// les panels doit passer par ici plut�t que d'appeler directement la Scene,
	// sinon elle devient invisible pour Ctrl+Z.
	class CommandHistory
	{
	public:
		void execute(std::unique_ptr<ICommand> command)
		{
			command->execute();
			m_UndoStack.push_back(std::move(command));
			m_RedoStack.clear();
		}

		// Construit la commande, l'ex�cute, et renvoie un pointeur brut pour
		// que l'appelant puisse lire son r�sultat (ex : l'entit� cr��e).
		// L'objet reste vivant tant qu'il n'est pas d�pil� de l'historique.
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

		// � appeler quand la sc�ne active change (nouvelle sc�ne, chargement) :
		// l'historique r�f�rence des entit�s de l'ancienne sc�ne, il devient invalide.
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
