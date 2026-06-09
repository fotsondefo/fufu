#include "EntryPoint.h"

namespace Fufu
{
	class FufuStudio : public Application
	{
	public:
		FufuStudio() : Application() {}

		~FufuStudio() = default;
	};
}


Fufu::Application* CreateApplication()
{
	return new Fufu::FufuStudio();
}