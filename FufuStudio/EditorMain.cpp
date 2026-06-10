#include <EntryPoint.h>
#include "Layers/StudioLayer.h"

class FufuStudioApp : public Fufu::Application
{
public:
	FufuStudioApp()
	{
		pushLayer(new FufuStudio::StudioLayer());
	}
};


Fufu::Application* CreateApplication()
{
	return new FufuStudioApp();
}