#pragma once

namespace FufuStudio
{
	// Interface to handle all reversible action in the editor
	class ICommand
	{
	public:
		virtual ~ICommand() = default;

		virtual void execute() = 0;
		virtual void undo() = 0;

		virtual void redo() { execute(); }

		virtual const char* getName() const { return "Action"; }
	};
}
