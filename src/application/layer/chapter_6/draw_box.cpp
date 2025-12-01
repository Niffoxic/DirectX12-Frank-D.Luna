#include "draw_box.h"

#include "imgui_impl_win32.h"
#include "framework/exception/dx_exception.h"
#include "framework/windows_manager/windows_manager.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

Draw3DBox::Draw3DBox(framework::DxRenderManager* manager)
	: IDrawLayer(manager)
{
	CreateImGuiDescriptorHeap();
	InitImgui();

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

	m_dirLight.Direction = DirectX::XMFLOAT3(-1.0f, -0.170f, 1.0f);
	m_dirLight.Color = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);

	m_pointLight.Position = DirectX::XMFLOAT3(-4.350f, 0.550f, -3.9f);
	m_pointLight.Range = 7.0f;
	m_pointLight.Color = DirectX::XMFLOAT3(1.0f, 0.9f, 0.7f);
}

void Draw3DBox::Draw(float deltaTime)
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Lighting Controls");

	if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat3("Dir Direction", &m_dirLight.Direction.x, 0.01f, -1.0f, 1.0f);
		ImGui::ColorEdit3("Dir Color", &m_dirLight.Color.x);
	}

	if (ImGui::CollapsingHeader("Point Light", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat3("Point Position", &m_pointLight.Position.x, 0.05f);
		ImGui::DragFloat("Point Range", &m_pointLight.Range, 0.1f, 0.1f, 500.0f);
		ImGui::ColorEdit3("Point Color", &m_pointLight.Color.x);
	}

	ImGui::End();

	Update(deltaTime);
	UpdateCamera(deltaTime);

	auto* render = m_pRender;
	auto* cmd = render->m_pCommandList.Get();

	render->m_pCommandAlloc->Reset();
	cmd->Reset(render->m_pCommandAlloc.Get(), m_pPipelineState.Get());

	D3D12_RESOURCE_BARRIER barrier;
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.pResource = render->GetBackBuffer();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	cmd->ResourceBarrier(1u, &barrier);

	cmd->RSSetScissorRects(1u, &render->m_scissorRect);
	cmd->RSSetViewports(1u, &render->m_viewport);

	constexpr float color[ 4 ]
	{
		0.f, 0.f, 0.f, 1.f
	};

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

	cmd->OMSetRenderTargets(1u, &handle, TRUE, &depthHandle);

	auto index = m_pGeometry->GetIndexViewDesc();
	auto vertex = m_pGeometry->GetVertexViewDesc();

	cmd->IASetIndexBuffer(&index);
	cmd->IASetVertexBuffers(0u, 1u, &vertex);
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D12DescriptorHeap* cbvHeaps[]{ m_pCbvHeap.Get() };
	cmd->SetDescriptorHeaps(1u, cbvHeaps);
	cmd->SetGraphicsRootSignature(m_pRootSignature.Get());
	cmd->SetGraphicsRootDescriptorTable(0u, m_pCbvHeap->GetGPUDescriptorHandleForHeapStart());

	cmd->DrawIndexedInstanced(
		m_pGeometry->Meshes[ "box" ].IndexCount,
		1u,
		0u,
		0u,
		0u
	);

	ImGui::Render();

	ID3D12DescriptorHeap* imguiHeaps[]{ m_imguiSrvHeap.Get() };
	cmd->SetDescriptorHeaps(1u, imguiHeaps);
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd);

	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

	cmd->ResourceBarrier(1u, &barrier);
	cmd->Close();

	ID3D12CommandList* cmdLists[]{ cmd };
	render->m_pCommandQueue->ExecuteCommandLists(1u, cmdLists);

	render->m_pSwapChain->Present(0u, 0u);
	render->m_nCurrentBackBuffer = (render->m_nCurrentBackBuffer + 1u) % render->SWAP_CHAIN_BUFFER_COUNT;

	render->FlushCommandQueue();
}

void Draw3DBox::Update(float deltaTime, bool f)
{
	m_nTimeElapsed += deltaTime;

	ConstantBufferDesc cb{};

	cb.TimeElapsed = m_nTimeElapsed;
	cb.Resolution = DirectX::XMFLOAT2(
		static_cast<float>(m_pRender->m_pWindowsManager->GetWindowsWidth()),
		static_cast<float>(m_pRender->m_pWindowsManager->GetWindowsHeight())
	);
	cb.Mouse = DirectX::XMFLOAT2(
		static_cast<float>(m_lastMousePosition.x),
		static_cast<float>(m_lastMousePosition.y)
	);

	using namespace DirectX;

	XMMATRIX world = XMLoadFloat4x4(&m_worldMatrix);
	if (!f)
	{
		world = world * DirectX::XMMatrixTranslation(4, 2, 0);
	}
	XMMATRIX view = XMLoadFloat4x4(&m_viewMatrix);
	XMMATRIX proj = XMLoadFloat4x4(&m_projMatrix);

	XMMATRIX wvp = world * view * proj;
	XMStoreFloat4x4(&cb.WorldViewProjectMatrix, XMMatrixTranspose(wvp));

	cb.EyePosW = m_eyePos;

	{
		XMVECTOR dir = XMLoadFloat3(&m_dirLight.Direction);
		dir = XMVector3Normalize(dir);

		XMStoreFloat3(&cb.DirLight.Direction, dir);
		cb.DirLight.Color = m_dirLight.Color;
	}

	{
		cb.PointLight.Position = m_pointLight.Position;
		cb.PointLight.Range = m_pointLight.Range;
		cb.PointLight.Color = m_pointLight.Color;
	}
	m_pCBResource->CopyData(0, cb);
}

void Draw3DBox::UpdateCamera(float deltaTime)
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

	XMStoreFloat4x4(&m_viewMatrix, view);
}

void Draw3DBox::InitImgui()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(m_pRender->m_pWindowsManager->GetWindowsHandle());

	DXGI_FORMAT rtvFormat = m_pRender->m_backBufferFormat;
	int         framesInFlight = m_pRender->SWAP_CHAIN_BUFFER_COUNT;

	ImGui_ImplDX12_Init(
		m_pRender->m_pDevice.Get(),
		framesInFlight,
		rtvFormat,
		m_imguiSrvHeap.Get(),
		m_imguiSrvHeap->GetCPUDescriptorHandleForHeapStart(),
		m_imguiSrvHeap->GetGPUDescriptorHandleForHeapStart()
	);

	unsigned char* pixels = nullptr;
	int width = 0, height = 0;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

	ImGui_ImplDX12_CreateDeviceObjects();
}

void Draw3DBox::CreateImGuiDescriptorHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NumDescriptors = 1;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	desc.NodeMask = 0;

	THROW_DX_IF_FAILS(m_pRender->m_pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_imguiSrvHeap)));
}

void Draw3DBox::BuildDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC desc;
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

	D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
	desc.BufferLocation = addr;
	desc.SizeInBytes    = size;

	m_pRender->m_pDevice->CreateConstantBufferView(
		&desc,
		m_pCbvHeap->GetCPUDescriptorHandleForHeapStart());
}

void Draw3DBox::BuildRootSignature()
{
	D3D12_DESCRIPTOR_RANGE cbvRange;
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

	D3D12_ROOT_SIGNATURE_DESC rootSigDesc;
	rootSigDesc.NumParameters	  = 1u;
	rootSigDesc.pParameters		  = rootParameters;
	rootSigDesc.NumStaticSamplers = 0u;
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
	m_inputLayout =
	{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

void Draw3DBox::BuildGeometry()
{
	constexpr int GRID_RES = 10; // 10x10 = 100 vertices

	std::vector<VertexDesc> vertices;
	vertices.reserve(GRID_RES * GRID_RES);

	float halfSize = 15.0f;
	float size = halfSize * 2.0f;

	// Generate a grid of vertices on the XZ plane
	for (int z = 0; z < GRID_RES; ++z)
	{
		float v = static_cast<float>(z) / (GRID_RES - 1);   // [0,1]
		float posZ = -halfSize + v * size;                    // [-15,15]

		for (int x = 0; x < GRID_RES; ++x)
		{
			float u = static_cast<float>(x) / (GRID_RES - 1); // [0,1]
			float posX = -halfSize + u * size;                  // [-15,15]

			vertices.push_back(
				VertexDesc{
					{ posX, 0.0f, posZ },   // Position
					{ 0.0f, 1.0f, 0.0f },   // Normal (up)
					{ 0.0f, 0.0f, 0.0f },   // Tangent (fill as you like)
					{ u, v }                // UVs
				}
			);
		}
	}

	std::vector<uint16_t> indices;
	indices.reserve((GRID_RES - 1) * (GRID_RES - 1) * 6);

	for (int z = 0; z < GRID_RES - 1; ++z)
	{
		for (int x = 0; x < GRID_RES - 1; ++x)
		{
			uint16_t i0 = static_cast<uint16_t>(z * GRID_RES + x);
			uint16_t i1 = static_cast<uint16_t>(z * GRID_RES + (x + 1));
			uint16_t i2 = static_cast<uint16_t>((z + 1) * GRID_RES + x);
			uint16_t i3 = static_cast<uint16_t>((z + 1) * GRID_RES + (x + 1));

			// Two triangles per quad
			// If backface culling is wrong, just swap the order.
			indices.push_back(i2); indices.push_back(i1); indices.push_back(i0); // tri 1
			indices.push_back(i1); indices.push_back(i2); indices.push_back(i3); // tri 2
		}
	}

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
		m_pRender->m_pDevice	 .Get (),
		m_pRender->m_pCommandList.Get (),
		vertices			     .data(),
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
							   static_cast<UINT>(m_inputLayout.size()) };
	psoDesc.pRootSignature = m_pRootSignature.Get();
	psoDesc.VS = 
	{
		static_cast<BYTE*>(m_pCompiledVS->GetBufferPointer()),
		m_pCompiledVS->GetBufferSize()
	};
	psoDesc.PS =
	{
		static_cast<BYTE*>(m_pCompiledPS->GetBufferPointer()),
		m_pCompiledPS->GetBufferSize()
	};
	psoDesc.RasterizerState			 = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	psoDesc.BlendState				 = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState		 = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask				 = UINT_MAX;
	psoDesc.PrimitiveTopologyType	 = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets		 = 1u;
	psoDesc.RTVFormats[ 0u ]		 = m_pRender->m_backBufferFormat;
	psoDesc.SampleDesc.Count		 = 1u;
	psoDesc.SampleDesc.Quality		 = 0u;
	psoDesc.DSVFormat				 = m_pRender->m_depthStencilFormat;

	THROW_DX_IF_FAILS(m_pRender->m_pDevice->CreateGraphicsPipelineState(
		&psoDesc,
		IID_PPV_ARGS(&m_pPipelineState)));
}
