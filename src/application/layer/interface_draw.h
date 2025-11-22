#pragma once
#include "framework/render_manager/render_manager.h"


class IDrawLayer
{
public:
	IDrawLayer(framework::DxRenderManager* manager)
		: m_pRender(manager)
	{
		m_pRender->AddDrawCB([&](float d)
		{
			Draw(d);
		});
	}
	virtual ~IDrawLayer() = default;
	virtual void Draw(float deltaTime) = 0;

protected:
	framework::DxRenderManager* m_pRender{ nullptr };
};
