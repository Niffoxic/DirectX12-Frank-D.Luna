#include "application.h"

namespace framework
{
	_Use_decl_annotations_
	Application::Application(framework::DX_FRAMEWORK_CONSTRUCT_DESC& desc)
		: framework::IFramework(desc)
	{
	}

	_Use_decl_annotations_
	bool Application::InitApplication()
	{
		//m_drawChapter4 = std::make_unique<InitDirectX>(m_pRenderManager.get());
		m_drawChapter6 = std::make_unique<Draw3DBox>(m_pRenderManager.get());
		return true;
	}

	_Use_decl_annotations_
	void Application::BeginPlay()
	{
	}

	_Use_decl_annotations_
	void Application::Release()
	{
	}

	_Use_decl_annotations_
	void Application::Tick(float deltaTime)
	{
		m_drawChapter6->Draw(deltaTime);
	}
}
