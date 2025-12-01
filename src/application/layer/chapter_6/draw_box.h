#pragma once

#include "application/layer/interface_draw.h"
#include <DirectXMath.h>

#include "utility/graphics/upload_buffer.h"
#include "utility/graphics/dx_utils.h"
#include "utility/graphics/math.h"

struct VertexDesc
{
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT3 Tangent;
	DirectX::XMFLOAT2 UV;
};

struct DirectionalLightCB
{
	DirectX::XMFLOAT3 Direction;
	float             pad0 = 0.0f;
	DirectX::XMFLOAT3 Color;
	float             pad1 = 0.0f;
};

struct PointLightCB
{
	DirectX::XMFLOAT3 Position;
	float             Range = 25.0f;
	DirectX::XMFLOAT3 Color;
	float             pad = 0.0f;
};

struct ConstantBufferDesc
{
	float TimeElapsed;
	float padding[ 3 ];

	DirectX::XMFLOAT2 Resolution;
	DirectX::XMFLOAT2 Mouse;

	DirectX::XMFLOAT4X4 WorldViewProjectMatrix;

	DirectX::XMFLOAT3 EyePosW;
	float             padEye = 0.0f;

	DirectionalLightCB DirLight;
	PointLightCB       PointLight;
};

class Draw3DBox : public IDrawLayer
{
public:
	Draw3DBox(framework::DxRenderManager* manager);
	~Draw3DBox() override = default;
	void Draw(float deltaTime) override;

private:
	void Update(float deltaTime, bool f=true);
	void UpdateCamera(float deltaTime);

	//~ build box
	void InitImgui();
	void CreateImGuiDescriptorHeap();
	void BuildDescriptorHeaps();
	void BuildConstantBuffers();
	void BuildRootSignature();
	void BuildShaders();
	void BuildInputLayout();
	void BuildGeometry();
	void BuildPSO();

private:
	Microsoft::WRL::ComPtr<ID3D12RootSignature>	 m_pRootSignature{ nullptr };
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pCbvHeap{ nullptr };

	std::unique_ptr<framework::UploadBuffer<ConstantBufferDesc>> m_pCBResource{ nullptr };
	std::unique_ptr<framework::MeshGeometry>                     m_pGeometry{ nullptr };

	Microsoft::WRL::ComPtr<ID3DBlob> m_pCompiledVS{ nullptr };
	Microsoft::WRL::ComPtr<ID3DBlob> m_pCompiledPS{ nullptr };

	std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pPipelineState{ nullptr };

	DirectX::XMFLOAT4X4 m_worldMatrix{ MathHelper::Identity4x4() };
	DirectX::XMFLOAT4X4 m_viewMatrix{ MathHelper::Identity4x4() };
	DirectX::XMFLOAT4X4 m_projMatrix{ MathHelper::Identity4x4() };
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_imguiSrvHeap;

	float m_nTheta{ 1.5f * DirectX::XM_PI };
	float m_nPhi{ DirectX::XM_PIDIV4 };
	float m_nRadius{ 5.0f };
	bool  m_cameraEnabled{ false };
	bool  m_spaceWasDown{ false };
	float m_nTimeElapsed{ 0.0f };
	DirectX::XMFLOAT3 m_eyePos{ 0.0f, 0.0f, -5.0f };
	float             m_yaw = 0.0f;
	float             m_pitch = 0.0f;

	struct
	{
		int x;
		int y;
	} m_lastMousePosition;

	DirectionalLightCB m_dirLight{};
	PointLightCB       m_pointLight{};
};
