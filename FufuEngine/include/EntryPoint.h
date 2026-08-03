#pragma once
#include "Application/Application.h"

extern Fufu::Application* CreateApplication();

int main(int /*argc*/, char** /*argv*/)
{
	auto* app = CreateApplication();
	app->run();
	delete app;
	return 0;
}