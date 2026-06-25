#include <EntryPoint.h>
#include "RuntimeConfigLoader.h"
#include "Layers/RuntimeLayer.h"

class FufuRuntimeApp : public Fufu::Application
{
public:
	explicit FufuRuntimeApp(const FufuRuntime::RuntimeConfig& config)
	{
		pushLayer(new FufuRuntime::RuntimeLayer(config));
	}
};

Fufu::Application* CreateApplication()
{
	// Résoudre le répertoire de l'exécutable
	std::filesystem::path exeDir = std::filesystem::current_path();

	auto config = FufuRuntime::RuntimeConfigLoader::load(exeDir);

	return new FufuRuntimeApp(config);
}