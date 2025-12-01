#pragma once

#include <cstdint>
#include <string>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <unordered_map>

#include "d3dx12.h"
#include "framework/exception/dx_exception.h"

namespace framework
{
	inline static Microsoft::WRL::ComPtr<ID3DBlob> CompileShader(
		const std::wstring& filename,
		const D3D_SHADER_MACRO* defines,
		const std::string& entrypoint,
		const std::string& target)
	{
		UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)  
		compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

		HRESULT hr = S_OK;

		Microsoft::WRL::ComPtr<ID3DBlob> byteCode = nullptr;
		Microsoft::WRL::ComPtr<ID3DBlob> errors;
		hr = D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
								entrypoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors);

		if (errors != nullptr)
		{
			OutputDebugStringA(static_cast<char*>(errors->GetBufferPointer()));
		}

		THROW_DX_IF_FAILS(hr);

		return byteCode;
	}

	typedef struct SubmeshGeometry
	{
		UINT IndexCount{ 0u };
		UINT StartIndex{ 0u };
		UINT BaseVertex{ 0u };
	} SubmeshGeometry;

	struct MeshGeometry
	{
		std::string Name;
		Microsoft::WRL::ComPtr<ID3DBlob>	   VertexBlob			 { nullptr };
		Microsoft::WRL::ComPtr<ID3DBlob>	   IndexBlob			 { nullptr };
		Microsoft::WRL::ComPtr<ID3D12Resource> VertexResource		 { nullptr };
		Microsoft::WRL::ComPtr<ID3D12Resource> IndexResource		 { nullptr };
		Microsoft::WRL::ComPtr<ID3D12Resource> VertexResourceUploader{ nullptr };
		Microsoft::WRL::ComPtr<ID3D12Resource> IndexResourceUploader { nullptr };

		//~ Data
		UINT		VertexByteStride	{ 0u };
		UINT		VertexBufferByteSize{ 0u };
		UINT	    IndexBufferByteSize	{ 0u };
		DXGI_FORMAT IndexFormat			{ DXGI_FORMAT_R16_UINT };

		std::unordered_map<std::string, SubmeshGeometry> Meshes{};

		D3D12_VERTEX_BUFFER_VIEW GetVertexViewDesc() const
		{
			D3D12_VERTEX_BUFFER_VIEW view{};
			view.BufferLocation = VertexResource->GetGPUVirtualAddress();
			view.SizeInBytes	= VertexBufferByteSize;
			view.StrideInBytes	= VertexByteStride;
			return view;
		}

		D3D12_INDEX_BUFFER_VIEW GetIndexViewDesc() const
		{
			D3D12_INDEX_BUFFER_VIEW view{};
			view.Format			= IndexFormat;
			view.SizeInBytes	= IndexBufferByteSize;
			view.BufferLocation = IndexResource->GetGPUVirtualAddress();
			return view;
		}

		void Reset()
		{
			VertexResource.Reset();
			IndexResource.Reset();
			VertexResource = nullptr;
			IndexResource  = nullptr;
		}
	};

    inline static Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
        ID3D12Device*               device,
        ID3D12GraphicsCommandList*  cmdList,
        const void*                 initData,
        UINT64                      byteSize,
        Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer)
    {
        using Microsoft::WRL::ComPtr;

        ComPtr<ID3D12Resource> defaultBuffer;

        D3D12_HEAP_PROPERTIES defaultHeap{};
        defaultHeap.Type                 = D3D12_HEAP_TYPE_DEFAULT;
        defaultHeap.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        defaultHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        defaultHeap.CreationNodeMask     = 1u;
        defaultHeap.VisibleNodeMask      = 1u;

        D3D12_RESOURCE_DESC bufferDesc{};
        bufferDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Alignment          = 0u;
        bufferDesc.Width              = byteSize;
        bufferDesc.Height             = 1u;
        bufferDesc.DepthOrArraySize   = 1u;
        bufferDesc.MipLevels          = 1u;
        bufferDesc.Format             = DXGI_FORMAT_UNKNOWN;
        bufferDesc.SampleDesc.Count   = 1u;
        bufferDesc.SampleDesc.Quality = 0u;
        bufferDesc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufferDesc.Flags              = D3D12_RESOURCE_FLAG_NONE;

        THROW_DX_IF_FAILS(
            device->CreateCommittedResource(
                &defaultHeap,
                D3D12_HEAP_FLAG_NONE,
                &bufferDesc,
                D3D12_RESOURCE_STATE_COMMON,
                nullptr,
                IID_PPV_ARGS(defaultBuffer.GetAddressOf()))
        );

        D3D12_HEAP_PROPERTIES uploadHeap{};
        uploadHeap.Type                 = D3D12_HEAP_TYPE_UPLOAD;
        uploadHeap.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        uploadHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        uploadHeap.CreationNodeMask     = 1u;
        uploadHeap.VisibleNodeMask      = 1u;

        THROW_DX_IF_FAILS(
            device->CreateCommittedResource(
                &uploadHeap,
                D3D12_HEAP_FLAG_NONE,
                &bufferDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(uploadBuffer.GetAddressOf()))
        );

        D3D12_SUBRESOURCE_DATA subResourceData{};
        subResourceData.pData       = initData;
        subResourceData.RowPitch    = byteSize;
        subResourceData.SlicePitch  = byteSize;

        D3D12_RESOURCE_BARRIER toCopyDest{};
        toCopyDest.Type                     = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toCopyDest.Flags                    = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        toCopyDest.Transition.pResource     = defaultBuffer.Get();
        toCopyDest.Transition.Subresource   = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        toCopyDest.Transition.StateBefore   = D3D12_RESOURCE_STATE_COMMON;
        toCopyDest.Transition.StateAfter    = D3D12_RESOURCE_STATE_COPY_DEST;

        cmdList->ResourceBarrier(1, &toCopyDest);

        UpdateSubresources<1>(
            cmdList,
            defaultBuffer.Get(),
            uploadBuffer.Get(),
            0u, 0u, 1u,
            &subResourceData
        );

        D3D12_RESOURCE_BARRIER toGenericRead{};
        toGenericRead.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        toGenericRead.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        toGenericRead.Transition.pResource   = defaultBuffer.Get();
        toGenericRead.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        toGenericRead.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        toGenericRead.Transition.StateAfter  = D3D12_RESOURCE_STATE_GENERIC_READ;

        cmdList->ResourceBarrier(1u, &toGenericRead);
        return defaultBuffer;
    }
}
