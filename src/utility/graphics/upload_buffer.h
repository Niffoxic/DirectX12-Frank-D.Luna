#pragma once

#include <cstdint>
#include <d3d12.h>
#include <wrl/client.h>

#include "framework/exception/dx_exception.h"


namespace framework
{
	enum class UploadBufferType : uint8_t
	{
		VertexIndexOrStructured = 0,
		Constant = 1
	};

	template<typename T>
	class UploadBuffer
	{
	public:
		UploadBuffer(ID3D12Device* device, UINT64 elementCounts, UploadBufferType type)
			: m_nElementCount(elementCounts), m_eBufferType(type)
		{
			assert(device && "Called to create upload buffer but D3D12::Device is nullptr!");
			assert(device && "Called to create upload buffer but element count is == 0!");

			m_nElementByteSize = static_cast<UINT64>(sizeof(T));

			//~ makes it multiple of 256 bytes 
			if (m_eBufferType == UploadBufferType::Constant)
			{
				m_nElementByteSize = (m_nElementByteSize + 255u) & ~255u;
			}

			const UINT64 bufferSize = m_nElementCount * m_nElementByteSize;

			D3D12_HEAP_PROPERTIES properties{};
			properties.Type					= D3D12_HEAP_TYPE_GPU_UPLOAD;
			properties.CPUPageProperty		= D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			properties.CreationNodeMask		= 1u;
			properties.VisibleNodeMask		= 1u;

			D3D12_RESOURCE_DESC resource{};
			resource.Dimension			= D3D12_RESOURCE_DIMENSION_BUFFER;
			resource.Alignment			= 0u;
			resource.Width				= bufferSize;
			resource.Height				= 1u;
			resource.DepthOrArraySize	= 1u;
			resource.MipLevels			= 1u;
			resource.Format				= DXGI_FORMAT_UNKNOWN;
			resource.SampleDesc.Count	= 1u;
			resource.SampleDesc.Quality = 0u;
			resource.Layout				= D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resource.Flags				= D3D12_RESOURCE_FLAG_NONE;

			THROW_DX_IF_FAILS(device->CreateCommittedResource(
				&properties,
				D3D12_HEAP_FLAG_NONE,
				&resource,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&m_pUploadResource)
			));

			//~ map the data
			THROW_DX_IF_FAILS(m_pUploadResource->Map(
				0u,
				nullptr,
				reinterpret_cast<void**>(&m_pMappedData)
			));
		}

		~UploadBuffer()
		{
			if (m_pUploadResource)
			{
				m_pUploadResource->Unmap(0u, nullptr);
			}
			m_pMappedData = nullptr;
		}

		UploadBuffer		   (const UploadBuffer&) = delete;
		UploadBuffer& operator=(const UploadBuffer&) = delete;

		//~ operations
		void CopyData(int elementIndex, const T& data)
		{
			memcpy(&m_pMappedData[ elementIndex * m_nElementByteSize ], &data, sizeof(T));
		}

		//~ Getters
		ID3D12Resource* GetResource() const { return m_pUploadResource.Get(); }

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> m_pUploadResource { nullptr };
		BYTE*								   m_pMappedData	 { nullptr };
		UINT64								   m_nElementByteSize{ 0u };
		UINT64								   m_nElementCount	 { 0u };
		UploadBufferType					   m_eBufferType	 { UploadBufferType::VertexIndexOrStructured };
	};
} // namespace framework

