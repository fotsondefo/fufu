#pragma once

#include "ICommand.h"
#include <memory>
#include <string>
#include <vector>

namespace FufuStudio
{
	// Groups multiple commands into a single history entry: ensures that an
	// operation on a multi-selection (deleting N entities, moving N entities
	// with the gizmo...) takes only one Ctrl+Z.
	// Sub-commands must be constructed but not yet executed:
	// CompositeCommand::execute() handles this, in the order they were added.
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
