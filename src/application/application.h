#pragma once

#include "framework/framework.h"
#include "layer/chapter_4/draw_init.h"
#include "layer/chapter_6/draw_box.h"
#include <memory>

namespace framework
{
	class Application final : public IFramework
	{
	public:
		Application(_In_opt_ DX_FRAMEWORK_CONSTRUCT_DESC& desc);
		~Application() override = default;

	protected:
		//~ Framework interface Impl
		_NODISCARD _Check_return_
		bool InitApplication() override;
		void BeginPlay() override;
		void Release() override;

		void Tick(_In_ float deltaTime) override;

	private:
		std::unique_ptr<InitDirectX> m_drawChapter4{ nullptr };
		std::unique_ptr<Draw3DBox>   m_drawChapter6{ nullptr };
	};
}
