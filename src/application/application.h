#pragma once

#include "framework/framework.h"

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
	};
}
