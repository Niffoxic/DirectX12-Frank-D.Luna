#include "draw_box.h"

#include "framework/exception/dx_exception.h"
#include "framework/windows_manager/windows_manager.h"

Draw3DBox::Draw3DBox(framework::DxRenderManager* manager)
	: IDrawLayer(manager)
{
	BuildDescriptorHeaps();
	BuildConstantBuffers();
	BuildRootSignature  ();
	BuildShaders		();
	BuildInputLayout	();

	//~ Build and Copy Geometry
	auto* render = m_pRender;
	render->m_pCommandAlloc->Reset();
	render->m_pCommandList->Reset(render->m_pCommandAlloc.Get(), nullptr);

	BuildGeometry();

	render->m_pCommandList->Close();
	ID3D12CommandList* cmdLists[] = { render->m_pCommandList.Get() };
	render->m_pCommandQueue->ExecuteCommandLists(1u, cmdLists);
	render->FlushCommandQueue();

	BuildPSO();

	DirectX::XMStoreFloat4x4(&m_worldMatrix, DirectX::XMMatrixIdentity());

	m_nRadius = 5.0f;
	m_nPhi = DirectX::XM_PIDIV4;
	m_nTheta = 1.5f * DirectX::XM_PI;

	auto proj = DirectX::XMMatrixPerspectiveFovLH(
		0.25f * DirectX::XM_PI,
		m_pRender->m_pWindowsManager->GetAspectRatio(),
		0.1f, 1000.f
	);
	DirectX::XMStoreFloat4x4(&m_projMatrix, proj);
}

void Draw3DBox::Draw(float deltaTime)
{
	Update(deltaTime);

	auto* render = m_pRender;
	auto* cmd = render->m_pCommandList.Get();
	
	render->m_pCommandAlloc->Reset();
	cmd->Reset(render->m_pCommandAlloc.Get(),m_pPipelineState.Get());

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type				   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags				   = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.pResource   = render->GetBackBuffer();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	cmd->ResourceBarrier(1u, &barrier);
	
	cmd->RSSetScissorRects(1u, &render->m_scissorRect);
	cmd->RSSetViewports(1u, &render->m_viewport);

	constexpr float color[]{ 0.24f, 0.11f, 0.65f, 1.0f };
	auto handle = render->GetBackBufferHandle();
	cmd->ClearRenderTargetView(handle, color, 0u, nullptr);

	auto depthHandle = render->GetDepthStencilHandle();
	cmd->ClearDepthStencilView(
		depthHandle,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		1.0f,
		0u,
		0u,
		nullptr
	);

	cmd->OMSetRenderTargets(
		1u,
		&handle,
		TRUE,
		&depthHandle);

	auto index = m_pGeometry->GetIndexViewDesc();
	cmd->IASetIndexBuffer(&index);
	auto vertex = m_pGeometry->GetVertexViewDesc();
	cmd->IASetVertexBuffers(0u, 1u, &vertex);
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D12DescriptorHeap* heaps[]{ m_pCbvHeap.Get() };
	UINT heapCount = ARRAYSIZE(heaps);
	cmd->SetDescriptorHeaps(heapCount, heaps);
	cmd->SetGraphicsRootSignature(m_pRootSignature.Get());
	cmd->SetGraphicsRootDescriptorTable(0u, m_pCbvHeap->GetGPUDescriptorHandleForHeapStart());

	cmd->DrawIndexedInstanced(
		m_pGeometry->Meshes[ "box" ].IndexCount,
		1u, 0u,
		0u, 0u);

	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;

	cmd->ResourceBarrier(1u, &barrier);
	cmd->Close();

	ID3D12CommandList* cmdLists[]{ cmd };
	render->m_pCommandQueue->ExecuteCommandLists(1u, cmdLists);

	render->m_pSwapChain->Present(0u, 0u);
	render->m_nCurrentBackBuffer = (render->m_nCurrentBackBuffer + 1u) % render->SWAP_CHAIN_BUFFER_COUNT;

	render->FlushCommandQueue();
}

void Draw3DBox::Update(const float deltaTime)
{
	m_nTimeElapsed += deltaTime;

	using namespace DirectX;

	float pos_x = m_nRadius * sinf(m_nPhi) * cosf(m_nTheta);
	float pos_z = m_nRadius * sinf(m_nPhi) * sinf(m_nTheta);
	float pos_y = m_nRadius * cosf(m_nPhi);

	XMVECTOR pos = XMVectorSet(pos_x, pos_y, pos_z - 5, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&m_viewMatrix, view);

	XMMATRIX world = XMLoadFloat4x4(&m_worldMatrix);
	XMMATRIX proj = XMLoadFloat4x4(&m_projMatrix);
	XMMATRIX worldViewProj = world * view * proj;

	ConstantBufferDesc desc{};
	XMStoreFloat4x4(
		&desc.WorldViewProjectMatrix,
		XMMatrixTranspose(worldViewProj)
	);

	int width = m_pRender->m_pWindowsManager->GetWindowsWidth();
	int height = m_pRender->m_pWindowsManager->GetWindowsHeight();

	desc.TimeElapsed = m_nTimeElapsed;
	desc.Resolution = { static_cast<float>(width),
						static_cast<float>(height) };

	int x, y;
	m_pRender->m_pWindowsManager->Mouse.GetMousePosition(x, y);

	desc.Mouse = { static_cast<float>(x), static_cast<float>(y) };

	m_pCBResource->CopyData(0u, desc);
}

void Draw3DBox::BuildDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC desc{};
	desc.Flags			= D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	desc.Type			= D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NodeMask		= 0u;
	desc.NumDescriptors = 1u;
	
	THROW_DX_IF_FAILS(m_pRender->m_pDevice->CreateDescriptorHeap(
		&desc,
		IID_PPV_ARGS(m_pCbvHeap.GetAddressOf())));

}

void Draw3DBox::BuildConstantBuffers()
{
	m_pCBResource = std::make_unique<framework::UploadBuffer<ConstantBufferDesc>>
		(m_pRender->m_pDevice.Get(),
		 1u, framework::UploadBufferType::Constant);

    constexpr UINT64		  size = (sizeof(ConstantBufferDesc) + 255u) & ~255u;
	D3D12_GPU_VIRTUAL_ADDRESS addr = m_pCBResource->GetResource()->GetGPUVirtualAddress();
	UINT64 boxIndex				   = 0u;
	
	addr += boxIndex * size;

	D3D12_CONSTANT_BUFFER_VIEW_DESC desc{};
	desc.BufferLocation = addr;
	desc.SizeInBytes    = size;

	m_pRender->m_pDevice->CreateConstantBufferView(
		&desc,
		m_pCbvHeap->GetCPUDescriptorHandleForHeapStart());
}

void Draw3DBox::BuildRootSignature()
{
	D3D12_DESCRIPTOR_RANGE cbvRange{};
	cbvRange.RangeType							= D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	cbvRange.NumDescriptors						= 1u;                         
	cbvRange.BaseShaderRegister					= 0u;                       
	cbvRange.RegisterSpace						= 0u;                
	cbvRange.OffsetInDescriptorsFromTableStart	= D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[ 1 ]{};
	rootParameters[ 0 ].ParameterType	 = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[ 0 ].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	rootParameters[ 0 ].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[ 0 ].DescriptorTable.pDescriptorRanges	= &cbvRange;

	D3D12_ROOT_SIGNATURE_DESC rootSigDesc{};
	rootSigDesc.NumParameters	  = 1;
	rootSigDesc.pParameters		  = rootParameters;
	rootSigDesc.NumStaticSamplers = 0;
	rootSigDesc.pStaticSamplers   = nullptr;
	rootSigDesc.Flags			  = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;


	Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSig;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

	HRESULT hr = D3D12SerializeRootSignature(
		&rootSigDesc,           
		D3D_ROOT_SIGNATURE_VERSION_1,     
		serializedRootSig.GetAddressOf(), 
		errorBlob.GetAddressOf()
	);

	if (errorBlob)
	{
		OutputDebugStringA(
			static_cast<const char*>(errorBlob->GetBufferPointer()));
	}

	THROW_DX_IF_FAILS(hr);
	THROW_DX_IF_FAILS(
		m_pRender->m_pDevice->CreateRootSignature(
			0u,                                    
			serializedRootSig->GetBufferPointer(),
			serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(&m_pRootSignature)
		)
	);
}

void Draw3DBox::BuildShaders()
{
	m_pCompiledVS = framework::CompileShader(L"shaders/chapter_6/vertex.hlsl", nullptr, "main", "vs_5_0");
	m_pCompiledPS = framework::CompileShader(L"shaders/chapter_6/pixel.hlsl",  nullptr, "main", "ps_5_0");
}

void Draw3DBox::BuildInputLayout()
{
	m_inputLayout = {
		{"Position", 0u, DXGI_FORMAT_R32G32B32_FLOAT, 0u, 0u, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0u},
		{"Color", 0u, DXGI_FORMAT_R32G32B32A32_FLOAT, 0u, 12u, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0u},
	};
}

void Draw3DBox::BuildGeometry()
{
	std::vector<VertexDesc> vertices =
	{
		// Front
		{ { -1, -1, -1 }, {1, 0, 0, 1} },
		{ { -1,  1, -1 }, {0, 1, 0, 1} },
		{ {  1,  1, -1 }, {0, 0, 1, 1} },
		{ {  1, -1, -1 }, {1, 1, 0, 1} },

		// Back
		{ { -1, -1,  1 }, {1, 0, 1, 1} },
		{ { -1,  1,  1 }, {0, 1, 1, 1} },
		{ {  1,  1,  1 }, {1, 1, 1, 1} },
		{ {  1, -1,  1 }, {0, 0, 0, 1} }
	};

	std::vector<uint16_t> indices =
	{
		// Front
		0,1,2, 0,2,3,
		// Back
		4,6,5, 4,7,6,
		// Left
		4,5,1, 4,1,0,
		// Right
		3,2,6, 3,6,7,
		// Top
		1,5,6, 1,6,2,
		// Bottom
		4,0,3, 4,3,7
	};

	const UINT vbSize = static_cast<UINT>(vertices.size()) * sizeof(VertexDesc);
	const UINT ibSize = static_cast<UINT>(indices.size())  * sizeof(uint16_t);
	
	m_pGeometry		  = std::make_unique<framework::MeshGeometry>();
	m_pGeometry->Name = "Box Body";

	THROW_DX_IF_FAILS(D3DCreateBlob(
		vbSize, &m_pGeometry->VertexBlob
	));
	CopyMemory(m_pGeometry->VertexBlob->GetBufferPointer(),
			   vertices.data(), vbSize);

	THROW_DX_IF_FAILS(D3DCreateBlob(
		ibSize, &m_pGeometry->IndexBlob
	));
	CopyMemory(m_pGeometry->IndexBlob->GetBufferPointer(),
			   indices.data(), ibSize);

	m_pGeometry->VertexByteStride	  = sizeof(VertexDesc);
	m_pGeometry->VertexBufferByteSize = vbSize;
	m_pGeometry->IndexFormat		  = DXGI_FORMAT_R16_UINT;
	m_pGeometry->IndexBufferByteSize  = ibSize;

	m_pGeometry->VertexResource = framework::CreateDefaultBuffer(
		m_pRender->m_pDevice.Get(),
		m_pRender->m_pCommandList.Get(),
		vertices.data(),
		vbSize,
		m_pGeometry->VertexResourceUploader);

	m_pGeometry->IndexResource = framework::CreateDefaultBuffer(
		m_pRender->m_pDevice.Get(),
		m_pRender->m_pCommandList.Get(),
		indices.data(),
		ibSize, m_pGeometry->IndexResourceUploader);

	framework::SubmeshGeometry submesh;
	submesh.IndexCount = static_cast<UINT>(indices.size());
	submesh.StartIndex = 0u;
	submesh.BaseVertex = 0u;

	m_pGeometry->Meshes[ "box" ] = std::move(submesh);
}

void Draw3DBox::BuildPSO()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.InputLayout = { m_inputLayout.data(),
							   (UINT)m_inputLayout.size() };
	psoDesc.pRootSignature = m_pRootSignature.Get();
	psoDesc.VS = {
		reinterpret_cast<BYTE*>(m_pCompiledVS->GetBufferPointer()),
		m_pCompiledVS->GetBufferSize()
	};
	psoDesc.PS = {
		reinterpret_cast<BYTE*>(m_pCompiledPS->GetBufferPointer()),
		m_pCompiledPS->GetBufferSize()
	};
	psoDesc.RasterizerState			 = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	psoDesc.BlendState				= CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState		= CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask				= UINT_MAX;
	psoDesc.PrimitiveTopologyType	= D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets		= 1;
	psoDesc.RTVFormats[ 0 ]			= m_pRender->m_backBufferFormat;
	psoDesc.SampleDesc.Count		= 1;
	psoDesc.SampleDesc.Quality		= 0;
	psoDesc.DSVFormat				= m_pRender->m_depthStencilFormat;

	THROW_DX_IF_FAILS(m_pRender->m_pDevice->CreateGraphicsPipelineState(
		&psoDesc,
		IID_PPV_ARGS(&m_pPipelineState)));
}
