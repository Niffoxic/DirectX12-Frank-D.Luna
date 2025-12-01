#include "FrameResource.h"

#include "framework/exception/dx_exception.h"
#include "utility/graphics/dx_utils.h"
#include "utility/graphics/upload_buffer.h"

FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount)
{
    THROW_DX_IF_FAILS(device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));

    PassCB   = std::make_unique<framework::UploadBuffer<PassConstants>>(device, passCount, framework::UploadBufferType::Constant);
    ObjectCB = std::make_unique<framework::UploadBuffer<ConstantData>> (device, objectCount, framework::UploadBufferType::Constant);
}
