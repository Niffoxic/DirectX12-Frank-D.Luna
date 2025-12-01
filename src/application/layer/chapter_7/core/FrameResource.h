#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <memory>
#include <DirectXMath.h>
#include "utility/graphics/math.h"
#include "utility/graphics/upload_buffer.h"

struct ConstantData
{
    float				TimeElapsed;
    float				padding[ 3 ];
    DirectX::XMFLOAT2   Resolution;
    DirectX::XMFLOAT2	MousePosition;
	DirectX::XMFLOAT4X4 World{ MathHelper::Identity4x4() };
};

struct alignas(16) PassConstants
{
    DirectX::XMFLOAT4X4 gView = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 gInvView = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 gProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 gInvProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 gViewProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 gInvViewProj = MathHelper::Identity4x4();

    DirectX::XMFLOAT3   gEyePosW = { 0.0f, 0.0f, 0.0f };
    float               cbPerObjectPad1 = 0.0f;

    DirectX::XMFLOAT2   gRenderTargetSize = { 0.0f, 0.0f };
    DirectX::XMFLOAT2   gInvRenderTargetSize = { 0.0f, 0.0f };

    float gNearZ = 0.0f;
    float gFarZ = 0.0f;
    float gTotalTime = 0.0f;
    float gDeltaTime = 0.0f;
};

struct Vertex
{
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT4 Color;
};

struct FrameResource
{
public:

    FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount);
    FrameResource(const FrameResource& rhs) = delete;
    FrameResource& operator=(const FrameResource& rhs) = delete;
    ~FrameResource() = default;

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>          CmdListAlloc;
    std::unique_ptr<framework::UploadBuffer<PassConstants>> PassCB   = nullptr;
    std::unique_ptr<framework::UploadBuffer<ConstantData>>  ObjectCB = nullptr;

    UINT64 Fence = 0;
};
