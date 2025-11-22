#include "framework.h"
#include "framework/exception/base_exception.h"
#include "framework/event/event_queue.h"
#include "framework/event/event_windows.h"

#include "utility/logger/logger.h"

namespace framework
{
	_Use_decl_annotations_
	IFramework::IFramework(const DX_FRAMEWORK_CONSTRUCT_DESC& desc)
	{
		if (!CreateManagers(desc))
		{
			logger::error("Failure in building manager!");
			return;
		}
		CreateUtilities();
		SubscribeToEvents();
	}

	IFramework::~IFramework()
	{
		if (m_pWindowsManager && !m_pWindowsManager->Release())
		{
			// TODO: Create Log record
		}
		logger::close();
	}

	_Use_decl_annotations_
	bool IFramework::Init()
	{
		InitManagers();

		if (!InitApplication())
		{
			logger::error("Failed to initialize application!");
			THROW_MSG("Failed to initialize application!");
			return false;
		}

		return true;
	}

	_Use_decl_annotations_
	HRESULT IFramework::Execute()
	{
		m_timer.ResetTime();
		logger::info("Starting Game Loop!");
		BeginPlay();
		while (true)
		{
			float dt = m_timer.Tick();
			if (m_bEnginePaused) dt = 0.0f;

			if (DxWindowsManager::ProcessMessages() == EProcessedMessageState::ExitMessage)
			{
				ReleaseManagers();
				return S_OK;
			}
			ManagerFrameBegin(dt);
			Tick(dt);
			ManagerFrameEnd();

#if defined(DEBUG) || defined(_DEBUG)
			static float passed = 0.0f;
			static int   frame = 0;
			static float avg_frames = 0.0f;
			static float last_time_elapsed = 0.0f;

			frame++;
			passed += dt;

			if (passed >= 1.0f)
			{
				avg_frames += frame;
				last_time_elapsed = m_timer.TimeElapsed();

				std::wstring message =
					L"Time Elapsed: " +
					std::to_wstring(last_time_elapsed) +
					L" Frame Rate: " +
					std::to_wstring(frame) +
					L" per second (Avg = " +
					std::to_wstring(avg_frames / last_time_elapsed) +
					L")";

				m_pWindowsManager->SetWindowMessageOnTitle(message);

				passed = 0.0f;
				frame = 0;
			}
#endif
			EventQueue::DispatchAll();
		}
		return S_OK;
	}

	_Use_decl_annotations_
	bool IFramework::CreateManagers(const DX_FRAMEWORK_CONSTRUCT_DESC& desc)
	{
		m_pWindowsManager = std::make_unique<DxWindowsManager>(desc.WindowsDesc);
		m_pRenderManager  = std::make_unique<DxRenderManager>(m_pWindowsManager.get());
		return true;
	}

	void IFramework::CreateUtilities()
	{
#if defined(_DEBUG) || defined(DEBUG)
		LOGGER_CREATE_DESC cfg{};
		cfg.TerminalName = "DirectX 12 Logger";
		logger::init(cfg);
#endif
	}

	void IFramework::InitManagers()
	{
		if (m_pWindowsManager && !m_pWindowsManager->Initialize())
		{
			logger::error("Failed to initialize Windows Manager!");
		}

		if (m_pRenderManager && !m_pRenderManager->Initialize())
		{
			logger::error("Failed to Initialize Render Manager");
		}

		logger::success("All Managers initialized.");
	}

	void framework::IFramework::ReleaseManagers()
	{
		logger::warning("Closing Application!");

		if (m_pWindowsManager && !m_pWindowsManager->Release())
		{
			logger::error("Failed to Release Windows Manager!");
		}

		if (m_pRenderManager && !m_pRenderManager->Release())
		{
			logger::error("Failed to release render manager");
		}

		logger::close();
	}

	_Use_decl_annotations_
	void IFramework::ManagerFrameBegin(float deltaTime)
	{
		if (m_pWindowsManager)
		{
			m_pWindowsManager->OnFrameBegin(deltaTime);
		}

		if (m_pRenderManager)
		{
			m_pRenderManager->OnFrameBegin(deltaTime);
		}
	}

	void IFramework::ManagerFrameEnd()
	{
		if (m_pWindowsManager)
		{
			m_pWindowsManager->OnFrameEnd();
		}
		if (m_pRenderManager)
		{
			m_pRenderManager->OnFrameEnd();
		}
	}

	void IFramework::SubscribeToEvents()
	{
		auto token = EventQueue::Subscribe<WINDOW_PAUSE_EVENT>(
			[&](const WINDOW_PAUSE_EVENT& event)
		{
			if (event.Paused) m_bEnginePaused = true;
			else
			{
				m_bEnginePaused = false;
				m_timer.ResetTime();
			}

			logger::debug("Window Drag Event Recevied with {}", event.Paused);
		});
	}

} // namespace framework
