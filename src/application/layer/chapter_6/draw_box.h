#pragma once

#include "application/layer/interface_draw.h"
#include <DirectXMath.h>

#include "utility/graphics/upload_buffer.h"
#include "utility/graphics/dx_utils.h"
#include "utility/graphics/math.h"

struct VertexDesc
{
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT4 Color;
};

struct ConstantBufferDesc
{
	float TimeElapsed;
	float padding[ 3 ];
	DirectX::XMFLOAT2	Resolution;
	DirectX::XMFLOAT2	Mouse;
	DirectX::XMFLOAT4X4 WorldViewProjectMatrix;
};

class Draw3DBox: public IDrawLayer
{
public:
	Draw3DBox(framework::DxRenderManager* manager);
	~Draw3DBox() override = default;
	void Draw(float deltaTime) override;

private:
	void Update(const float deltaTime);

	//~ build box
	void BuildDescriptorHeaps();
	void BuildConstantBuffers();
	void BuildRootSignature  ();
	void BuildShaders		 ();
	void BuildInputLayout	 ();
	void BuildGeometry		 ();
	void BuildPSO			 ();

private:
	Microsoft::WRL::ComPtr<ID3D12RootSignature>				 m_pRootSignature{ nullptr };
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>			 m_pCbvHeap		 { nullptr };
	
	std::unique_ptr<framework::UploadBuffer<ConstantBufferDesc>> m_pCBResource	 { nullptr };
	std::unique_ptr<framework::MeshGeometry>				     m_pGeometry	 { nullptr };

	Microsoft::WRL::ComPtr<ID3DBlob> m_pCompiledVS{ nullptr };
	Microsoft::WRL::ComPtr<ID3DBlob> m_pCompiledPS{ nullptr };

	std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pPipelineState{ nullptr };

	DirectX::XMFLOAT4X4 m_worldMatrix{ MathHelper::Identity4x4() };
	DirectX::XMFLOAT4X4 m_viewMatrix { MathHelper::Identity4x4() };
	DirectX::XMFLOAT4X4 m_projMatrix { MathHelper::Identity4x4() };

	float m_nTheta		{ 1.5f * DirectX::XM_PI };
	float m_nPhi	    { DirectX::XM_PIDIV4 };
	float m_nRadius	    { 5.0f };
	float m_nTimeElapsed{ 0.0f };

	struct
	{
		int x;
		int y;
	} m_lastMousePosition;
};
