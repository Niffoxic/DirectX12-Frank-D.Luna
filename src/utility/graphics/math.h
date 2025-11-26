#pragma once
#include <DirectXMath.h>


class MathHelper
{
public:
	static DirectX::XMFLOAT4X4 Identity4x4()
	{
		return 
		{
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f 
			};
	}
};
