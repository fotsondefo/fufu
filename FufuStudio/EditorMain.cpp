#include <EntryPoint.h>
#include "Layers/StudioLayer.h"

class FufuStudioApp : public Fufu::Application
{
public:
	explicit FufuStudioApp() 
		: Fufu::Application(
			Fufu::WindowProps{
				"Fufu Engine",
				1280,
				720
			}
		)
	{
		pushLayer(new FufuStudio::StudioLayer());
	}
};


Fufu::Application* CreateApplication()
{
	return new FufuStudioApp();
}