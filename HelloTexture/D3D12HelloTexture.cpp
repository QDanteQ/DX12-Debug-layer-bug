//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "stdafx.h"
#include "D3D12HelloTexture.h"

D3D12HelloTexture::D3D12HelloTexture(UINT width, UINT height, std::wstring name) :
    DXSample(width, height, name),
    m_frameIndex(0),
    m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
    m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
    m_rtvDescriptorSize(0),
    m_DepthFormat(DXGI_FORMAT_D32_FLOAT_S8X24_UINT) //DXGI_FORMAT_D24_UNORM_S8_UINT
{
}

void D3D12HelloTexture::OnInit()
{
    LoadPipeline();
    LoadAssets();
}

// Load the rendering pipeline dependencies.
void D3D12HelloTexture::LoadPipeline()
{
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();

            // Enable additional debug layers.
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    if (m_useWarpDevice)
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

        ThrowIfFailed(D3D12CreateDevice(
            warpAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
            ));
    }
    else
    {
        ComPtr<IDXGIAdapter1> hardwareAdapter;
        GetHardwareAdapter(factory.Get(), &hardwareAdapter);

        ThrowIfFailed(D3D12CreateDevice(
            hardwareAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
            ));
    }

    // Describe and create the command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
        Win32Application::GetHwnd(),
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain
        ));

    // This sample does not support fullscreen transitions.
    ThrowIfFailed(factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain.As(&m_swapChain));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Create descriptor heaps.
    {
        // Describe and create a render target view (RTV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = 1;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

        
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.NumDescriptors = 2;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_DSVDescHeap)));

        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    }

    // Create frame resources.
    //{
    //    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

    //    // Create a RTV for each frame.
    //    for (UINT n = 0; n < FrameCount; n++)
    //    {
    //        ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
    //        m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
    //        rtvHandle.Offset(1, m_rtvDescriptorSize);
    //    }
    //}

    ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
}

// Load the sample assets.
void D3D12HelloTexture::LoadAssets()
{
    // Create the root signature.
    {
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

        // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

        if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
        {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init_1_1(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
        ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
    }

    // Create the pipeline state, which includes compiling and loading shaders.
    {
        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT compileFlags = 0;
#endif

        ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
        ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

        // Define the vertex input layout.
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        psoDesc.pRootSignature = m_rootSignature.Get();
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.StencilEnable = TRUE;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.DSVFormat = m_DepthFormat;
        psoDesc.SampleDesc.Count = 4;
        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
    }

    // Create the command list.
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));

    ThrowIfFailed(m_commandList.As(&m_commandList1));

    // Create the vertex buffer.
    {
        // Define the geometry for a triangle.
        Vertex triangleVertices[] =
        {
            { { 0.0f, 0.25f * m_aspectRatio, 0.0f }, { 0.5f, 0.0f } },
            { { 0.25f, -0.25f * m_aspectRatio, 0.0f }, { 1.0f, 1.0f } },
            { { -0.25f, -0.25f * m_aspectRatio, 0.0f }, { 0.0f, 1.0f } }
        };

        const UINT vertexBufferSize = sizeof(triangleVertices);

        // Note: using upload heaps to transfer static data like vert buffers is not 
        // recommended. Every time the GPU needs it, the upload heap will be marshalled 
        // over. Please read up on Default Heap usage. An upload heap is used here for 
        // code simplicity and because there are very few verts to actually transfer.
        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_vertexBuffer)));

        // Copy the triangle data to the vertex buffer.
        UINT8* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
        m_vertexBuffer->Unmap(0, nullptr);

        // Initialize the vertex buffer view.
        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.StrideInBytes = sizeof(Vertex);
        m_vertexBufferView.SizeInBytes = vertexBufferSize;
    }

    // Create the texture.
    {
        // Describe and create a Texture2D.
        D3D12_RESOURCE_DESC textureDesc = {};
        textureDesc.MipLevels = 1;
        textureDesc.Format = m_DepthFormat; 
        textureDesc.Width = TextureWidth;
        textureDesc.Height = TextureHeight;
        textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        textureDesc.DepthOrArraySize = 1;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &textureDesc,
            D3D12_RESOURCE_STATE_DEPTH_READ,
            nullptr,
            IID_PPV_ARGS(&m_depthTexture)));

        m_depthTexture->SetName(L"depthNonMSAATexture");
        textureDesc.SampleDesc.Count = 4;

		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			nullptr,
			IID_PPV_ARGS(&m_depthMSAATexture)));
        
        m_depthMSAATexture->SetName(L"depthMSAATexture");

		textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			nullptr,
			IID_PPV_ARGS(&m_renderMSAATexture)));

        m_renderMSAATexture->SetName(L"renderMSAATexture");
    }

    {		
        D3D12_DEPTH_STENCIL_VIEW_DESC desc = {m_DepthFormat, D3D12_DSV_DIMENSION_TEXTURE2D, D3D12_DSV_FLAG_NONE };
        
        m_dsvOnDepth = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_DSVDescHeap->GetCPUDescriptorHandleForHeapStart());
        m_device->CreateDepthStencilView(m_depthTexture.Get(), &desc, m_dsvOnDepth);
        
        desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
        m_dsvOnMSAADepth = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_DSVDescHeap->GetCPUDescriptorHandleForHeapStart(), 1, m_dsvDescriptorSize);
        m_device->CreateDepthStencilView(m_depthMSAATexture.Get(), &desc, m_dsvOnMSAADepth);
    }
    {
        D3D12_RENDER_TARGET_VIEW_DESC desc = { DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RTV_DIMENSION_TEXTURE2DMS };
		m_rtvOnMSAARender = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
		m_device->CreateRenderTargetView(m_renderMSAATexture.Get(), &desc, m_rtvOnMSAARender);
    }
    
    // Close the command list and execute it to begin the initial GPU setup.
    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    {
        ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fenceValue = 1;

        // Create an event handle to use for frame synchronization.
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        // Wait for the command list to execute; we are reusing the same command 
        // list in our main loop but for now, we just want to wait for setup to 
        // complete before continuing.
        WaitForPreviousFrame();
    }
}

// Update frame-based values.
void D3D12HelloTexture::OnUpdate()
{
}

void D3D12HelloTexture::ExecuteAndWait()
{
	ThrowIfFailed(m_commandList1->Close());

	// Execute the command list.
	ID3D12CommandList* ppCommandLists[] = { m_commandList1.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	
    WaitForPreviousFrame();
}

// Render the scene.
void D3D12HelloTexture::OnRender()
{
    // Record all the commands we need to render the scene into the command list.
    ClearRenderPass();
    ExecuteAndWait();

    DrawRenderPass();
	ExecuteAndWait();

    // Present the frame.
    //ThrowIfFailed(m_swapChain->Present(1, 0));
}

void D3D12HelloTexture::OnDestroy()
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    WaitForPreviousFrame();

    CloseHandle(m_fenceEvent);
}

void D3D12HelloTexture::ClearRenderPass()
{
    ThrowIfFailed(m_commandAllocator->Reset());
    ThrowIfFailed(m_commandList1->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

	m_commandList1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_depthTexture.Get(), D3D12_RESOURCE_STATE_DEPTH_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE));

    // Record commands.
    m_commandList1->ClearDepthStencilView(m_dsvOnDepth, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0u, 0, nullptr);
    {
        float clearValue[] = { 0.f,0.f,0.f,0.f };
        m_commandList1->ClearRenderTargetView(m_rtvOnMSAARender, clearValue, 0, nullptr);
    }
    m_commandList1->SetSamplePositions(4, 1, customSamplePositions);
    m_commandList1->ClearDepthStencilView(m_dsvOnMSAADepth, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.f, 0u, 0, nullptr);

    m_commandList1->SetSamplePositions(0, 0, nullptr); // Revert to default. Redundant here, as we execute asap.
}

void D3D12HelloTexture::DrawRenderPass()
{
	ThrowIfFailed(m_commandAllocator->Reset());
	ThrowIfFailed(m_commandList1->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

	// Set necessary state.
	m_commandList1->SetGraphicsRootSignature(m_rootSignature.Get());
	m_commandList1->RSSetViewports(1, &m_viewport);
	m_commandList1->RSSetScissorRects(1, &m_scissorRect);

	m_commandList1->OMSetRenderTargets(1, &m_rtvOnMSAARender, FALSE, &m_dsvOnMSAADepth);

	m_commandList1->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_commandList1->IASetVertexBuffers(0, 1, &m_vertexBufferView);

	m_commandList1->SetSamplePositions(4, 1, customSamplePositions);
	m_commandList1->DrawInstanced(3, 1, 0, 0);

	// Some work with default sample pos on the same cmdlist
	m_commandList1->SetSamplePositions(0, 0, nullptr); // Revert to default. Redundant here, as we execute asap.
	m_commandList1->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_depthTexture.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_DEPTH_READ));
}

void D3D12HelloTexture::WaitForPreviousFrame()
{
    // WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
    // This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
    // sample illustrates how to use fences for efficient resource usage and to
    // maximize GPU utilization.

    // Signal and increment the fence value.
    const UINT64 fence = m_fenceValue;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
    m_fenceValue++;

    // Wait until the previous frame is finished.
    if (m_fence->GetCompletedValue() < fence)
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}
