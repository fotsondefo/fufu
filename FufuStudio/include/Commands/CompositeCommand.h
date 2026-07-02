#pragma once

#include "ICommand.h"
#include <memory>
#include <string>
#include <vector>

namespace FufuStudio
{
	// Regroupe plusieurs commandes en une seule entrée d'historique : sert à ce
	// qu'une opération sur une multi-sélection (supprimer N entités, déplacer N
	// entités avec le gizmo...) ne prenne qu'un seul Ctrl+Z.
	// Les sous-commandes doivent être construites mais pas encore exécutées :
	// c'est CompositeCommand::execute() qui s'en charge, dans l'ordre d'ajout.
	class CompositeCommand : public ICommand
	{
	public:
		explicit CompositeCommand(std::string name) : m_Name(std::move(name)) {}

		void add(std::unique_ptr<ICommand> command) { m_Commands.push_back(std::move(command)); }
		bool empty() const { return m_Commands.empty(); }

		void execute() override
		{
			for (auto& command : m_Commands)
				command->execute();
		}

		void undo() override
		{
			for (auto it = m_Commands.rbegin(); it != m_Commands.rend(); ++it)
				(*it)->undo();
		}

		void redo() override
		{
			for (auto& command : m_Commands)
				command->redo();
		}

		const char* getName() const override { return m_Name.c_str(); }

	private:
		std::string m_Name;
		std::vector<std::unique_ptr<ICommand>> m_Commands;
	};
}
