#include "application.h"

namespace framework
{
	Application::Application(framework::DX_FRAMEWORK_CONSTRUCT_DESC& desc)
		: framework::IFramework(desc)
	{
	}

	bool Application::InitApplication()
	{
		return true;
	}

	void Application::BeginPlay()
	{
	}

	void Application::Release()
	{
	}

	void Application::Tick(float deltaTime)
	{
	}
}
