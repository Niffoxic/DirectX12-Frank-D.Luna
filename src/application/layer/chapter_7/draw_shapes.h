#pragma once
#include <DirectXMath.h>

#include "application/layer/interface_draw.h"
#include "core/FrameResource.h"
#include "utility/graphics/dx_utils.h"
#include "utility/graphics/math.h"

struct RenderItem
{
	RenderItem() = default;

	framework::MeshGeometry* Geometry{ nullptr };
	D3D12_PRIMITIVE_TOPOLOGY Topology{ D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
	DirectX::XMFLOAT4X4		 World	 { MathHelper::Identity4x4() };

	//~ Draw Config
	UINT FramesDirty	   { 3u };
	UINT ObjectCBIndex	   { 0u };
	UINT IndexCount		   { 0u };
	UINT StartIndexLocation{ 0u };
	UINT BaseVertexLocation{ 0u };
};

class DrawShapes final: public IDrawLayer
{
public:
	DrawShapes(framework::DxRenderManager* manager);
	~DrawShapes() override;

	DrawShapes(const DrawShapes&) = delete;
	DrawShapes(DrawShapes&&)	  = delete;

	DrawShapes& operator=(const DrawShapes&) = delete;
	DrawShapes& operator=(DrawShapes&&)		 = delete;

	//~ IDraw Impl
	void Draw(float deltaTime) override;

private:
	//~ Per frame updates
	void Update			 (float deltaTime);
	void UpdateCamera	 (float deltaTime);
	void HandleInput	 (float deltaTime);
	void UpdateObjectCBs (float deltaTime);
	void UpdateMainPassCB(float deltaTime);

	//~ Build/Create Resources
	void BuildDescriptorHeaps	 ();
	void BuildConstantBufferViews();
	void BuildRootSignature		 ();
	void BuildShaders			 ();
	void BuildInputLayout		 ();
	void BuildGeometry			 ();
	void BuildPipeline			 ();
	void BuildFrameResources	 ();
	void BuildRenderItems		 ();
	
	//~ draws
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList,
						 const std::vector<RenderItem*>& items);
private:
	//~ fixed
	const UINT nFrameResourcesMaxCount{ 3u };

	std::vector<std::unique_ptr<FrameResource>> m_ppFrameResources{};
	FrameResource* m_pCurrentFrameResource{ nullptr };
	UINT m_nCurrentFrameIndex{ 0u };

	Microsoft::WRL::ComPtr<ID3D12RootSignature>  m_pRootSignature{ nullptr };
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pCbHeap		 { nullptr };
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pSrvHeap		 { nullptr };

	//~ resources
	std::vector<std::unique_ptr<RenderItem>> m_ppRenderItems{};
	std::vector<RenderItem*>				 m_ppOpaqueItems {};
	PassConstants m_mainPassCB{};
	UINT m_nPassCBOffset{ 0u };
	bool m_bWireFrame	{ false };
	float m_wireToggleTimer = 0.0f;
	float m_yaw = 0.0f;
	float m_pitch = 0.0f;

	DirectX::XMFLOAT3 m_eyePos = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT4X4 m_view = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 m_proj = MathHelper::Identity4x4();

	float m_nTheta  = 1.5f * DirectX::XM_PI;
	float m_nPhi	= 0.2f * DirectX::XM_PI;
	float m_nRadius = 15.0f;
	float m_nTimeElapsed{ 0.f };
	bool  m_cameraEnabled{ false };
	bool  m_spaceWasDown{ false };

	//~ maps
	std::unordered_map<std::string, std::unique_ptr<framework::MeshGeometry>> m_geometries{};
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> m_compiledShaders   {};
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> m_pso    {};

	//~ configurations
	std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout{};
};
