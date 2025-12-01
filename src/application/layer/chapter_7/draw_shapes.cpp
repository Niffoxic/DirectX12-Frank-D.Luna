#include "draw_shapes.h"

#include "framework/windows_manager/windows_manager.h"
#include "utility/graphics/geometry_generator.h"
#include "utility/logger/logger.h"

DrawShapes::DrawShapes(framework::DxRenderManager* manager)
	: IDrawLayer(manager)
{
	BuildRootSignature		();
	BuildShaders			();
	BuildInputLayout		();

	auto* render = m_pRender;
	render->m_pCommandAlloc->Reset();
	render->m_pCommandList->Reset(render->m_pCommandAlloc.Get(), nullptr);

	BuildGeometry			();
	BuildRenderItems		();
	BuildFrameResources		();
	BuildDescriptorHeaps	();
	BuildConstantBufferViews();
	BuildPipeline			();

	THROW_DX_IF_FAILS(render->m_pCommandList->Close());
	ID3D12CommandList* lists[]{ render->m_pCommandList.Get() };
	UINT size = ARRAYSIZE(lists);
	render->m_pCommandQueue->ExecuteCommandLists(size, lists);
	render->FlushCommandQueue();

	m_nRadius = 5.0f;
	m_nPhi = DirectX::XM_PIDIV4;
	m_nTheta = 1.5f * DirectX::XM_PI;

	m_eyePos = { 0.0f, 3.0f, -10.0f };
	m_yaw = 0.0f;
	m_pitch = 0.0f;

	auto proj = DirectX::XMMatrixPerspectiveFovLH(
		0.25f * DirectX::XM_PI,
		m_pRender->m_pWindowsManager->GetAspectRatio(),
		0.1f, 1000.f
	);
	DirectX::XMStoreFloat4x4(&m_proj, proj);
}

DrawShapes::~DrawShapes()
{
}

void DrawShapes::Draw(float deltaTime)
{
	m_nTimeElapsed += deltaTime;
	Update(deltaTime);

	auto cmdList = m_pRender->m_pCommandList.Get();
	auto cmdListAlloc = m_pCurrentFrameResource->CmdListAlloc.Get();
	THROW_DX_IF_FAILS(cmdListAlloc->Reset());

	if (m_bWireFrame)
	{
		THROW_DX_IF_FAILS(cmdList->Reset(cmdListAlloc, m_pso[ "opaque_wireframe" ].Get()));
	}else
	{
		THROW_DX_IF_FAILS(cmdList->Reset(cmdListAlloc, m_pso[ "opaque" ].Get()));
	}

	cmdList->RSSetViewports	  (1u, &m_pRender->m_viewport);
	cmdList->RSSetScissorRects(1u, &m_pRender->m_scissorRect);

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Flags					= D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Type					= D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.StateBefore	= D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter	= D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.Subresource	= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.pResource	= m_pRender->GetBackBuffer();
	cmdList->ResourceBarrier(1u, &barrier);

	auto handle = m_pRender->GetBackBufferHandle();
	constexpr float color[]{ 0.25f, 0.26f, 0.71f, 1.0f };
	cmdList->ClearRenderTargetView(handle, color, 0u, nullptr);

	auto dHandle = m_pRender->GetDepthStencilHandle();
	cmdList->ClearDepthStencilView(dHandle,
								   D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
								   1.0f, 0u, 0u, nullptr);
	cmdList->OMSetRenderTargets(1u,
								&handle,
								true,
								&dHandle);
	ID3D12DescriptorHeap* heaps[]{ m_pCbHeap.Get() };
	cmdList->SetDescriptorHeaps(1u, heaps);
	cmdList->SetGraphicsRootSignature(m_pRootSignature.Get());

	UINT passIndex = m_nPassCBOffset + m_nCurrentFrameIndex;
	auto passHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_pCbHeap->GetGPUDescriptorHandleForHeapStart());
	passHandle.Offset(passIndex, m_pRender->m_nCbvSrvUavDescriptorSize);
	cmdList->SetGraphicsRootDescriptorTable(1u, passHandle);

	DrawRenderItems(cmdList, m_ppOpaqueItems);

	//~ ready for present
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
	cmdList->ResourceBarrier(1u, &barrier);

	THROW_DX_IF_FAILS(cmdList->Close());
	ID3D12CommandList* cmdLists[]{ cmdList };
	m_pRender->m_pCommandQueue->ExecuteCommandLists(1u, cmdLists);
	m_pRender->m_pSwapChain->Present(0u, 0u);

	m_pRender->m_nCurrentBackBuffer =
		(m_pRender->m_nCurrentBackBuffer + 1u) % m_pRender->SWAP_CHAIN_BUFFER_COUNT;

	m_pCurrentFrameResource->Fence = ++m_pRender->m_nCurrentFence;

	m_pRender->m_pCommandQueue->Signal(m_pRender->m_pFence.Get(),
									   m_pRender->m_nCurrentFence);
}

void DrawShapes::Update(float deltaTime)
{
	HandleInput(deltaTime);
	UpdateCamera(deltaTime);

	m_nCurrentFrameIndex = (m_nCurrentFrameIndex + 1u) % nFrameResourcesMaxCount;
	m_pCurrentFrameResource = m_ppFrameResources[ m_nCurrentFrameIndex ].get();

	auto* fence = m_pRender->m_pFence.Get();
	if (m_pCurrentFrameResource->Fence != 0 &&
		fence->GetCompletedValue() < m_pCurrentFrameResource->Fence)
	{
		HANDLE event = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
		THROW_DX_IF_FAILS(fence->SetEventOnCompletion(m_pCurrentFrameResource->Fence, event));
		WaitForSingleObject(event, INFINITE);
		CloseHandle(event);
	}
	
	UpdateObjectCBs (deltaTime);
	UpdateMainPassCB(deltaTime);
}

void DrawShapes::UpdateCamera(float deltaTime)
{
	using namespace DirectX;

	auto& keyboard = m_pRender->m_pWindowsManager->Keyboard;
	auto& mouse = m_pRender->m_pWindowsManager->Mouse;

	bool spaceNow = keyboard.IsKeyPressed(' ');
	if (spaceNow && !m_spaceWasDown)
	{
		m_cameraEnabled = !m_cameraEnabled;

		if (m_cameraEnabled)
		{
			mouse.HideCursor();
			mouse.LockCursorToWindow();
		} else
		{
			mouse.UnHideCursor();
			mouse.UnlockCursor();
		}
	}
	m_spaceWasDown = spaceNow;

	int dx = 0, dy = 0;
	mouse.GetMouseDelta(dx, dy); // always clear raw delta

	if (!m_cameraEnabled)
		return;

	const float mouseSensitivity = 0.0025f;

	m_yaw += dx * mouseSensitivity;
	m_pitch -= dy * mouseSensitivity;

	const float pitchLimit = 0.99f * XM_PIDIV2;
	m_pitch = std::clamp(m_pitch, -pitchLimit, pitchLimit);

	float cosPitch = std::cosf(m_pitch);
	float sinPitch = std::sinf(m_pitch);
	float cosYaw = std::cosf(m_yaw);
	float sinYaw = std::sinf(m_yaw);

	XMVECTOR forward = XMVectorSet(
		cosPitch * sinYaw,
		sinPitch,
		cosPitch * cosYaw,
		0.0f
	);

	XMVECTOR up = XMVectorSet(0.f, 1.f, 0.f, 0.f);
	XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, forward));

	XMVECTOR pos = XMLoadFloat3(&m_eyePos);

	const float moveSpeed = 5.0f;
	float       dt = deltaTime;

	if (keyboard.IsKeyPressed('W')) pos += forward * (moveSpeed * dt);
	if (keyboard.IsKeyPressed('S')) pos -= forward * (moveSpeed * dt);
	if (keyboard.IsKeyPressed('A')) pos -= right * (moveSpeed * dt);
	if (keyboard.IsKeyPressed('D')) pos += right * (moveSpeed * dt);

	XMStoreFloat3(&m_eyePos, pos);

	XMVECTOR target = pos + forward;
	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);

	XMStoreFloat4x4(&m_view, view);
}

void DrawShapes::HandleInput(float deltaTime)
{
	if (m_wireToggleTimer > 0.0f)
		m_wireToggleTimer -= deltaTime;

	auto& keyboard = m_pRender->m_pWindowsManager->Keyboard;

	if (keyboard.IsKeyPressed('T') && m_wireToggleTimer <= 0.0f)
	{
		m_bWireFrame = !m_bWireFrame;
		logger::debug("Called Wire Frame to: {}", m_bWireFrame);
		m_wireToggleTimer = 0.25f;
	}
}

void DrawShapes::UpdateObjectCBs(float deltaTime)
{
	auto currentObjectCB = m_pCurrentFrameResource->ObjectCB.get();

	for (auto& item: m_ppRenderItems)
	{
			DirectX::XMMATRIX world = XMLoadFloat4x4(&item->World);
			auto* windows = m_pRender->m_pWindowsManager;
			auto& mouse = windows->Mouse;
			int x, y;
			mouse.GetMousePosition(x, y);

			ConstantData data{};
			DirectX::XMStoreFloat4x4(&data.World, DirectX::XMMatrixTranspose(world));
			data.MousePosition = { static_cast<float>(x), static_cast<float>(y) };
			data.Resolution.x = windows->GetWindowsWidth();
			data.Resolution.y = windows->GetWindowsHeight();
			data.TimeElapsed  = m_nTimeElapsed;
			
			currentObjectCB->CopyData(item->ObjectCBIndex, data);
			--item->FramesDirty;
		
	}
}

void DrawShapes::UpdateMainPassCB(float deltaTime)
{
	using namespace DirectX;

	XMMATRIX view	  = XMLoadFloat4x4(&m_view);
	XMMATRIX proj	  = XMLoadFloat4x4(&m_proj);
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);

	XMMATRIX invView	 = XMMatrixInverse(nullptr, view);
	XMMATRIX invProj	 = XMMatrixInverse(nullptr, proj);
	XMMATRIX invViewProj = XMMatrixInverse(nullptr, viewProj);

	XMStoreFloat4x4(&m_mainPassCB.gView, XMMatrixTranspose(view));
	XMStoreFloat4x4(&m_mainPassCB.gInvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&m_mainPassCB.gProj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&m_mainPassCB.gInvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&m_mainPassCB.gViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&m_mainPassCB.gInvViewProj, XMMatrixTranspose(invViewProj));

	m_mainPassCB.gEyePosW = m_eyePos;
	m_mainPassCB.cbPerObjectPad1 = 0.0f;

	auto* windows = m_pRender->m_pWindowsManager;
	float w = static_cast<float>(windows->GetWindowsWidth());
	float h = static_cast<float>(windows->GetWindowsHeight());

	m_mainPassCB.gRenderTargetSize	  = DirectX::XMFLOAT2(w, h);
	m_mainPassCB.gInvRenderTargetSize = DirectX::XMFLOAT2(1.0f / w, 1.0f / h);

	m_mainPassCB.gNearZ		= 1.0f;
	m_mainPassCB.gFarZ	    = 1000.0f;
	m_mainPassCB.gTotalTime = m_nTimeElapsed;
	m_mainPassCB.gDeltaTime = deltaTime;

	auto currPassCB = m_pCurrentFrameResource->PassCB.get();
	currPassCB->CopyData(0, m_mainPassCB);
}

void DrawShapes::BuildDescriptorHeaps()
{
	UINT counts		  = static_cast<UINT>(m_ppOpaqueItems.size());
	UINT nDescriptors = (counts + 1u) * nFrameResourcesMaxCount;
	m_nPassCBOffset   = counts * nFrameResourcesMaxCount;

	D3D12_DESCRIPTOR_HEAP_DESC desc{};
	desc.Flags			= D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	desc.Type			= D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NodeMask		= 0u;
	desc.NumDescriptors = nDescriptors;

	THROW_DX_IF_FAILS(m_pRender->m_pDevice->CreateDescriptorHeap(
		&desc,
		IID_PPV_ARGS(&m_pCbHeap)));
}

void DrawShapes::BuildConstantBufferViews()
{
	UINT objCBByteSize = (static_cast<UINT>(sizeof(ConstantData)) + 255u) & ~255u;

	UINT objCount = static_cast<UINT>(m_ppOpaqueItems.size());

	auto* device = m_pRender->m_pDevice.Get();

	for (int frameIndex = 0; frameIndex < nFrameResourcesMaxCount; ++frameIndex)
	{
		auto objectCB = m_ppFrameResources[ frameIndex ]->ObjectCB->GetResource();
		for (UINT i = 0; i < objCount; ++i)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = objectCB->GetGPUVirtualAddress();

			cbAddress += i * objCBByteSize;

			int heapIndex = frameIndex * objCount + i;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_pCbHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, m_pRender->m_nCbvSrvUavDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = objCBByteSize;

			device->CreateConstantBufferView(&cbvDesc, handle);
		}
	}

	UINT passCBByteSize = (static_cast<UINT>(sizeof(PassConstants)) + 255u) & ~255u;

	// Last three descriptors are the pass CBVs for each frame resource.
	for (UINT frameIndex = 0; frameIndex < nFrameResourcesMaxCount; ++frameIndex)
	{
		auto passCB = m_ppFrameResources[ frameIndex ]->PassCB->GetResource();
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress();

		// Offset to the pass cbv in the descriptor heap.
		UINT heapIndex = m_nPassCBOffset + frameIndex;
		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_pCbHeap->GetCPUDescriptorHandleForHeapStart());
		handle.Offset(heapIndex, m_pRender->m_nCbvSrvUavDescriptorSize);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = cbAddress;
		cbvDesc.SizeInBytes    = passCBByteSize;

		device->CreateConstantBufferView(&cbvDesc, handle);
	}
}

void DrawShapes::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE cbvTable0;
	cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE cbvTable1;
	cbvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);

	CD3DX12_ROOT_PARAMETER slotRootParameter[ 2 ];

	slotRootParameter[ 0 ].InitAsDescriptorTable(1, &cbvTable0);
	slotRootParameter[ 1 ].InitAsDescriptorTable(1, &cbvTable1);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter, 0, nullptr,
											D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSig = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
											 serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
	}
	THROW_DX_IF_FAILS(hr);

	THROW_DX_IF_FAILS(m_pRender->m_pDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(m_pRootSignature.GetAddressOf())));
}

void DrawShapes::BuildShaders()
{
	m_compiledShaders[ "standardVS" ] = framework::CompileShader(
		L"shaders/chapter_7/vertex.hlsl",
		nullptr,
		"main",
		"vs_5_0");

	m_compiledShaders[ "opaquePS" ] = framework::CompileShader(
		L"shaders/chapter_7/pixel.hlsl",
		nullptr,
		"main",
		"ps_5_0");
}

void DrawShapes::BuildInputLayout()
{
	m_inputLayout =
	{
		{ "POSITION", 0,
			DXGI_FORMAT_R32G32B32_FLOAT, 0,
			0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0 },
		{ "COLOR", 0,
			DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
			12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0 },
	};
}

void DrawShapes::BuildGeometry()
{
	auto* device = m_pRender->m_pDevice.Get();
	using namespace DirectX;
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.5f, 0.5f, 1.5f, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
	GeometryGenerator::MeshData cylinder = geoGen.CreateSphere(0.5f, 20, 20);

	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
	UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();

	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
	UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();


	framework::SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndex = boxIndexOffset;
	boxSubmesh.BaseVertex = boxVertexOffset;

	framework::SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.StartIndex = gridIndexOffset;
	gridSubmesh.BaseVertex = gridVertexOffset;

	framework::SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndex = sphereIndexOffset;
	sphereSubmesh.BaseVertex = sphereVertexOffset;

	framework::SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.StartIndex = cylinderIndexOffset;
	cylinderSubmesh.BaseVertex = cylinderVertexOffset;

	auto totalVertexCount =
		box.Vertices.size() +
		grid.Vertices.size() +
		sphere.Vertices.size() +
		cylinder.Vertices.size();

	std::vector<Vertex> vertices(totalVertexCount);

	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[ k ].Position = box.Vertices[ i ].Position;
		vertices[ k ].Color = XMFLOAT4(0.25f, 0.51f, 0.81f, 1.0f);
	}

	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[ k ].Position = grid.Vertices[ i ].Position;
		vertices[ k ].Color = XMFLOAT4(0.45f, 0.11f, 0.21f, 1.0f);
	}

	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[ k ].Position = sphere.Vertices[ i ].Position;
		vertices[ k ].Color = XMFLOAT4(0.75f, 0.71f, 0.51f, 1.0f);
	}

	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[ k ].Position = cylinder.Vertices[ i ].Position;
		vertices[ k ].Color = XMFLOAT4(0.15f, 0.41f, 0.56f, 1.0f);
	}

	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<framework::MeshGeometry>();
	geo->Name = "shapeGeo";

	THROW_DX_IF_FAILS(D3DCreateBlob(vbByteSize, &geo->VertexBlob));
	CopyMemory(geo->VertexBlob->GetBufferPointer(), vertices.data(), vbByteSize);

	THROW_DX_IF_FAILS(D3DCreateBlob(ibByteSize, &geo->IndexBlob));
	CopyMemory(geo->IndexBlob->GetBufferPointer(), indices.data(), ibByteSize);

	auto* cmdList = m_pRender->m_pCommandList.Get();

	geo->VertexResource = framework::CreateDefaultBuffer(device,
														 cmdList,
														 vertices.data(),
														 vbByteSize,
														 geo->VertexResourceUploader);

	geo->IndexResource = framework::CreateDefaultBuffer(device,
														cmdList,
														indices.data(),
														ibByteSize,
														geo->IndexResourceUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->Meshes[ "box" ] = boxSubmesh;
	geo->Meshes[ "grid" ] = gridSubmesh;
	geo->Meshes[ "sphere" ] = sphereSubmesh;
	geo->Meshes[ "cylinder" ] = cylinderSubmesh;

	m_geometries[ geo->Name ] = std::move(geo);
}

void DrawShapes::BuildPipeline()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc{};
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	opaquePsoDesc.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size() };
	opaquePsoDesc.pRootSignature = m_pRootSignature.Get();
	opaquePsoDesc.VS = {
		reinterpret_cast<BYTE*>(m_compiledShaders[ "standardVS" ]->GetBufferPointer()),
		m_compiledShaders[ "standardVS" ]->GetBufferSize()
	};
	opaquePsoDesc.PS = {
		reinterpret_cast<BYTE*>(m_compiledShaders[ "opaquePS" ]->GetBufferPointer()),
		m_compiledShaders[ "opaquePS" ]->GetBufferSize()
	};

	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;

	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[ 0 ] = m_pRender->m_backBufferFormat;
	opaquePsoDesc.SampleDesc.Count = 1;
	opaquePsoDesc.SampleDesc.Quality = 0;
	opaquePsoDesc.DSVFormat = m_pRender->m_depthStencilFormat;

	THROW_DX_IF_FAILS(m_pRender->m_pDevice->CreateGraphicsPipelineState(
		&opaquePsoDesc, IID_PPV_ARGS(&m_pso[ "opaque" ])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
	opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;

	THROW_DX_IF_FAILS(m_pRender->m_pDevice->CreateGraphicsPipelineState(
		&opaqueWireframePsoDesc, IID_PPV_ARGS(&m_pso[ "opaque_wireframe" ])));
}

void DrawShapes::BuildFrameResources()
{
	for (int i = 0; i < nFrameResourcesMaxCount; ++i)
	{
		m_ppFrameResources.push_back(std::make_unique<FrameResource>
									 (m_pRender->m_pDevice.Get(),
										1u,
									  (UINT)m_ppRenderItems.size()));
	}
}

void DrawShapes::BuildRenderItems()
{
	using namespace DirectX;

	auto boxRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem->World, XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(0.0f, 0.5f, 0.0f));
	boxRitem->ObjectCBIndex = 0;
	boxRitem->Geometry = m_geometries[ "shapeGeo" ].get();
	boxRitem->Topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geometry->Meshes[ "box" ].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geometry->Meshes[ "box" ].StartIndex;
	boxRitem->BaseVertexLocation = boxRitem->Geometry->Meshes[ "box" ].BaseVertex;
	m_ppRenderItems.push_back(std::move(boxRitem));

	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->World = MathHelper::Identity4x4();
	gridRitem->ObjectCBIndex = 1;
	gridRitem->Geometry = m_geometries[ "shapeGeo" ].get();
	gridRitem->Topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->IndexCount = gridRitem->Geometry->Meshes[ "grid" ].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geometry->Meshes[ "grid" ].StartIndex;
	gridRitem->BaseVertexLocation = gridRitem->Geometry->Meshes[ "grid" ].BaseVertex;
	m_ppRenderItems.push_back(std::move(gridRitem));

	UINT objCBIndex = 2;
	for (int i = 0; i < 5; ++i)
	{
		auto leftCylRitem = std::make_unique<RenderItem>();
		auto rightCylRitem = std::make_unique<RenderItem>();
		auto leftSphereRitem = std::make_unique<RenderItem>();
		auto rightSphereRitem = std::make_unique<RenderItem>();

		XMMATRIX leftCylWorld = XMMatrixTranslation(-5.0f, 1.5f, -10.0f + i * 5.0f);
		XMMATRIX rightCylWorld = XMMatrixTranslation(+5.0f, 1.5f, -10.0f + i * 5.0f);

		XMMATRIX leftSphereWorld = XMMatrixTranslation(-5.0f, 3.5f, -10.0f + i * 5.0f);
		XMMATRIX rightSphereWorld = XMMatrixTranslation(+5.0f, 3.5f, -10.0f + i * 5.0f);

		XMStoreFloat4x4(&leftCylRitem->World, rightCylWorld);
		leftCylRitem->ObjectCBIndex = objCBIndex++;
		leftCylRitem->Geometry = m_geometries[ "shapeGeo" ].get();
		leftCylRitem->Topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftCylRitem->IndexCount = leftCylRitem->Geometry->Meshes[ "cylinder" ].IndexCount;
		leftCylRitem->StartIndexLocation = leftCylRitem->Geometry->Meshes[ "cylinder" ].StartIndex;
		leftCylRitem->BaseVertexLocation = leftCylRitem->Geometry->Meshes[ "cylinder" ].BaseVertex;

		XMStoreFloat4x4(&rightCylRitem->World, leftCylWorld);
		rightCylRitem->ObjectCBIndex = objCBIndex++;
		rightCylRitem->Geometry = m_geometries[ "shapeGeo" ].get();
		rightCylRitem->Topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightCylRitem->IndexCount = rightCylRitem->Geometry->Meshes[ "cylinder" ].IndexCount;
		rightCylRitem->StartIndexLocation = rightCylRitem->Geometry->Meshes[ "cylinder" ].StartIndex;
		rightCylRitem->BaseVertexLocation = rightCylRitem->Geometry->Meshes[ "cylinder" ].BaseVertex;

		XMStoreFloat4x4(&leftSphereRitem->World, leftSphereWorld);
		leftSphereRitem->ObjectCBIndex = objCBIndex++;
		leftSphereRitem->Geometry = m_geometries[ "shapeGeo" ].get();
		leftSphereRitem->Topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftSphereRitem->IndexCount = leftSphereRitem->Geometry->Meshes[ "sphere" ].IndexCount;
		leftSphereRitem->StartIndexLocation = leftSphereRitem->Geometry->Meshes[ "sphere" ].StartIndex;
		leftSphereRitem->BaseVertexLocation = leftSphereRitem->Geometry->Meshes[ "sphere" ].BaseVertex;

		XMStoreFloat4x4(&rightSphereRitem->World, rightSphereWorld);
		rightSphereRitem->ObjectCBIndex = objCBIndex++;
		rightSphereRitem->Geometry = m_geometries[ "shapeGeo" ].get();
		rightSphereRitem->Topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightSphereRitem->IndexCount = rightSphereRitem->Geometry->Meshes[ "sphere" ].IndexCount;
		rightSphereRitem->StartIndexLocation = rightSphereRitem->Geometry->Meshes[ "sphere" ].StartIndex;
		rightSphereRitem->BaseVertexLocation = rightSphereRitem->Geometry->Meshes[ "sphere" ].BaseVertex;

		m_ppRenderItems.push_back(std::move(leftCylRitem));
		m_ppRenderItems.push_back(std::move(rightCylRitem));
		m_ppRenderItems.push_back(std::move(leftSphereRitem));
		m_ppRenderItems.push_back(std::move(rightSphereRitem));
	}

	for (auto& e : m_ppRenderItems)
		m_ppOpaqueItems.push_back(e.get());
}

void DrawShapes::DrawRenderItems(
	ID3D12GraphicsCommandList* cmdList,
	const std::vector<RenderItem*>& items)
{
	UINT objCBByteSize = (static_cast<UINT>(sizeof(ConstantData)) + 255u) & ~255u;

	auto objectCB = m_pCurrentFrameResource->ObjectCB->GetResource();

	for (size_t i = 0; i < items.size(); ++i)
	{
		//if (items[ i ]->Geometry->Meshes.contains("sphere")) continue;

		auto ri = items[ i ];

		auto vDesc = ri->Geometry->GetVertexViewDesc();
		auto iDesc = ri->Geometry->GetIndexViewDesc();
		cmdList->IASetVertexBuffers(0, 1, &vDesc);
		cmdList->IASetIndexBuffer(&iDesc);
		cmdList->IASetPrimitiveTopology(ri->Topology);

		// Offset to the CBV in the descriptor heap for this object and for this frame resource.
		UINT cbvIndex = m_nCurrentFrameIndex * (UINT)m_ppOpaqueItems.size() + ri->ObjectCBIndex;
		auto cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_pCbHeap->GetGPUDescriptorHandleForHeapStart());
		cbvHandle.Offset(cbvIndex, m_pRender->m_nCbvSrvUavDescriptorSize);

		cmdList->SetGraphicsRootDescriptorTable(0, cbvHandle);

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}
