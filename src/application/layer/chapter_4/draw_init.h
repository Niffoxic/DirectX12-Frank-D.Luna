#pragma once

#include "application/layer/interface_draw.h"

class InitDirectX : public IDrawLayer
{
public:
	InitDirectX(framework::DxRenderManager* manager);
	~InitDirectX() override = default;
	void Draw(float deltaTime) override;
};
