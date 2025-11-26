#include "draw_init.h"
#include "framework/exception/dx_exception.h"

#include <cmath>

using namespace framework;

InitDirectX::InitDirectX(framework::DxRenderManager* manager)
	: IDrawLayer(manager)
{}

void InitDirectX::Draw(float deltaTime)
{
	static float totalTime = 0.0f;
	totalTime += deltaTime;

	THROW_DX_IF_FAILS(m_pRender->m_pCommandAlloc->Reset());
	THROW_DX_IF_FAILS(m_pRender->m_pCommandList->Reset(m_pRender->m_pCommandAlloc.Get(), nullptr));

	ID3D12Resource* pBackBuffer = m_pRender->GetBackBuffer();

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type					= D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags					= D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource	= pBackBuffer;
	barrier.Transition.Subresource	= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore	= D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter	= D3D12_RESOURCE_STATE_RENDER_TARGET;
	
	m_pRender->m_pCommandList->ResourceBarrier(1u, &barrier);
	m_pRender->m_pCommandList->RSSetViewports(1u, &m_pRender->m_viewport);
	m_pRender->m_pCommandList->RSSetScissorRects(1u, &m_pRender->m_scissorRect);

	const float color[4]
	{
		std::sinf(totalTime),
		std::cosf(totalTime),
		std::sinf(std::sinf(totalTime) + std::cosf(totalTime)),
		std::sinf(totalTime)
	};
	
	m_pRender->m_pCommandList->ClearRenderTargetView(m_pRender->GetBackBufferHandle(), color, 0u, nullptr);
	m_pRender->m_pCommandList->ClearDepthStencilView(m_pRender->GetDepthStencilHandle(),
													 D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
													 1.f, 0u, 0u, nullptr);
	auto bHandle = m_pRender->GetBackBufferHandle();
	auto dHandle = m_pRender->GetDepthStencilHandle();
	m_pRender->m_pCommandList->OMSetRenderTargets(1u,
												  &bHandle,
												  TRUE,
												  &dHandle);
	barrier = {};
	barrier.Type					= D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags					= D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource	= pBackBuffer;
	barrier.Transition.Subresource	= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore	= D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter	= D3D12_RESOURCE_STATE_PRESENT;
	m_pRender->m_pCommandList->ResourceBarrier(1u, &barrier);

	THROW_DX_IF_FAILS(m_pRender->m_pCommandList->Close());
	ID3D12CommandList* cmdLists[] = { m_pRender->m_pCommandList.Get() };
	
	m_pRender->m_pCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
	THROW_DX_IF_FAILS(m_pRender->m_pSwapChain->Present(0u, 0u));

	m_pRender->m_nCurrentBackBuffer = (m_pRender->m_nCurrentBackBuffer + 1) % m_pRender->SWAP_CHAIN_BUFFER_COUNT;
	m_pRender->FlushCommandQueue();
}
