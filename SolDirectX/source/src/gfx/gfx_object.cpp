#include "../../header/gfx/gfx_object.h"
#include <vector>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <iostream>
#include "../../header/d3dUtil.h"
#include "../../header/d3dx12.h"


#define LOG_ADAPTER_OUTPUTS false    //是否打印配置器的显示参数列表
#define LOG_DEVICE_FUNC_SUPPORT true //是否打印设备的功能支持

#pragma comment(lib,"d3dcompiler.lib")
// 链接到dxgi库
#pragma comment(lib, "dxgi.lib")
// 链接到d3d12
#pragma comment(lib, "D3D12.lib")

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;

bool LittleGFXInstance::Initialize(bool enableDebugLayer)
{
    debugLayerEnabled = enableDebugLayer;
    UINT flags = 0;
    if (debugLayerEnabled) flags = DXGI_CREATE_FACTORY_DEBUG;
    if (SUCCEEDED(CreateDXGIFactory2(flags, IID_PPV_ARGS(&pDXGIFactory))))
    {
        queryAllAdapters();
        // If the only adapter we found is a software adapter, log error message for QA
        if (!adapters.size() && foundSoftwareAdapter)
        {
            assert(0 && "The only available GPU has DXGI_ADAPTER_FLAG_SOFTWARE. Early exiting");
            return false;
        }
    }
    else
    {
        assert("[D3D12 Fatal]: Create DXGIFactory2 Failed!");
        return false;
    }
    return true;
}

bool LittleGFXInstance::Destroy()
{
    for (auto iter : adapters)
    {
        SAFE_RELEASE(iter.pDXGIAdapter);
    }
    SAFE_RELEASE(pDXGIFactory);
    return true;
}

//输出每个适配器的参数
void LogOutputDisplayMods(IDXGIOutput* output,DXGI_FORMAT format) {
    UINT count = 0;
    UINT flags = 0;

    //以nullptr作为参数调用此函数来获取符合条件的显示模式的个数
    output->GetDisplayModeList(format, flags, &count, nullptr);

    std::vector<DXGI_MODE_DESC> modeList(count);
    output->GetDisplayModeList(format, flags, &count, &modeList[0]);

    for (auto x : modeList) {
        UINT n = x.RefreshRate.Numerator;
        UINT d = x.RefreshRate.Denominator;
        std::wstring text =
            L"Width = " + std::to_wstring(x.Width) + L" " +
            L"Height = " + std::to_wstring(x.Height) + L" " +
            L"Refresh = " + std::to_wstring(n) + L"/" + std::to_wstring(d) +
            L"\n";
        std::wcout << "显示参数为: " << text << std::endl;
    }

}

//打印适配器的显示配置
void LogAdaptersOutputs(IDXGIAdapter* adapter)
{
    UINT i = 0;
    DXGI_ADAPTER_DESC desc = {};
    adapter->GetDesc(&desc);
    std::wcout << "找到硬件适配器: " << desc.Description << std::endl;

    if (LOG_ADAPTER_OUTPUTS) {
        IDXGIOutput* output = nullptr;
        //输出每个适配器对于R8G8B8A8的适配
        while (adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND) {
            LogOutputDisplayMods(output, DXGI_FORMAT_R8G8B8A8_UNORM);
            i++;
        }
    }
}

//设备功能检测支持
//详见: https://learn.microsoft.com/zh-cn/windows/win32/api/d3d12/ne-d3d12-d3d12_feature
void LogDeviceSupport(ID3D12Device* device)
{
    D3D_FEATURE_LEVEL featureLevels[5] = {
        D3D_FEATURE_LEVEL_12_0, //是否支持D3D12
        D3D_FEATURE_LEVEL_12_1, //是否支持D3D12
        D3D_FEATURE_LEVEL_11_0, //是否支持D3D11
        D3D_FEATURE_LEVEL_10_0, //是否支持D3D10
        D3D_FEATURE_LEVEL_9_3, //是否支持D3D19.3
    };
    std::string strs[5] = {
        "12.0","12.1","11.0","10.0","9.3"
    };

    for (int i = 0; i < 5; ++i) {
        D3D_FEATURE_LEVEL m_featureLevels[1] = { featureLevels[i] };
        D3D12_FEATURE_DATA_FEATURE_LEVELS featureLevelsInfo;
        featureLevelsInfo.NumFeatureLevels = 1;
        featureLevelsInfo.pFeatureLevelsRequested = m_featureLevels;
        HRESULT result = device->CheckFeatureSupport(
            D3D12_FEATURE_FEATURE_LEVELS,
            &featureLevelsInfo,
            sizeof(featureLevelsInfo)
        );
        if (result != DXGI_ERROR_NOT_FOUND) {
            std::cout << "支持DirectX " << strs[i] << " " << std::endl;
        }
    }

    //检查是否支持硬件光追
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 optionsInfo;
    device->CheckFeatureSupport(
        D3D12_FEATURE_D3D12_OPTIONS5,
        &optionsInfo,
        sizeof(optionsInfo)
    );
    if (optionsInfo.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED) {
        std::cout << "支持硬件光追" << std::endl;
    }
}

void LittleGFXInstance::queryAllAdapters()
{
    IDXGIAdapter4* adapter = NULL;
    // Use DXGI6 interface which lets us specify gpu preference so we dont need to use NVOptimus or AMDPowerExpress
    for (UINT i = 0;
        pDXGIFactory->EnumAdapterByGpuPreference(i,
            DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND;
        i++)
    {
        LittleGFXAdapter newAdapter;
        newAdapter.pDXGIAdapter = adapter;
        newAdapter.instance = this;
        DXGI_ADAPTER_DESC3 desc = {};
        adapter->GetDesc3(&desc);

        LogAdaptersOutputs(adapter);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            foundSoftwareAdapter = true;
            SAFE_RELEASE(adapter);
        }
        else
        {
            adapters.emplace_back(newAdapter);
        }
    }
}

bool LittleGFXDevice::Initialize(LittleGFXAdapter* in_adapter)
{
    this->adapter = in_adapter;
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_12_0;
    if (!SUCCEEDED(D3D12CreateDevice(adapter->pDXGIAdapter, // default adapter
        featureLevel, IID_PPV_ARGS(&pD3D12Device))))
    {
        assert(0 && "[D3D12 Fatal]: Create D3D12Device Failed!");
        return false;
    }

    if (LOG_DEVICE_FUNC_SUPPORT) {
        LogDeviceSupport(pD3D12Device);
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    if (!SUCCEEDED(pD3D12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&pD3D12Queue))))
    {
        assert(0 && "[D3D12 Fatal]: Create D3D12CommandQueue Failed!");
        return false;
    }
    return true;
}

bool LittleGFXDevice::Destroy()
{
    SAFE_RELEASE(pD3D12Queue);
    SAFE_RELEASE(pD3D12Device);
    return true;
}





LittleGFXWindow* LittleGFXWindow::mWindow = nullptr;
LittleGFXWindow* LittleGFXWindow::GetWindow() {
    return mWindow;
}

LittleGFXWindow::LittleGFXWindow() {
    mWindow = this;
}

LittleGFXWindow::~LittleGFXWindow() {
    if (md3dDevice != nullptr) {
        FlushCommandQueue();
    }
}

bool LittleGFXWindow::Get4xMsaaState() const {
    return m4xMsaaState;
}

void LittleGFXWindow::Set4xMsaaState(bool value) {
    if (m4xMsaaState != value) {
        m4xMsaaState = value;

        //重構SwapChain
        CreateSwapChain();
        //OnResize();
    }
}

bool LittleGFXWindow::Initialize(const wchar_t* title, LittleGFXDevice* device, bool enableVsync)
{
    auto succeed = LittleWindow::Initialize(title);

    if (!InitDirect3D()) {
        std::cout << "初始化Direct3D失败" << std::endl;
        succeed = false;
    }
    
    return succeed;
}

bool LittleGFXWindow::InitDirect3D() {
#if defined(DEBUG) || defined(_DEBUG)
    //開啓Debug层,todo
#endif
    //IID_PPV_ARGS是用于寻找ComPtr的唯一Id的
    ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mdxgiFactory)));
    
    //Try to create hardware device
    HRESULT hardwareResult = D3D12CreateDevice(
        nullptr,
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&md3dDevice)
    );

    if (FAILED(hardwareResult)) {
        ComPtr<IDXGIAdapter> pWrapAdapter;
        ThrowIfFailed(mdxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWrapAdapter)));

        ThrowIfFailed(D3D12CreateDevice(
            pWrapAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&md3dDevice)));
    }
    //创建Fence
    ThrowIfFailed(md3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));

    //从Device上获取对应的描述符的大小(硬件)
    //渲染目标视图
    mRtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    //深度缓冲区视图
    mDsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    //常量缓冲区视图,着色器资源视图和无序访问视图组合的描述符堆
    mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    //Check 4x MSAA quality support for our back buffer format.
    //All Direct3D 11 capable devices support 4X MSAA for all render
    //target formats,so we only need to check quality support.
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
    msQualityLevels.Format = mBackBufferFormat;
    msQualityLevels.SampleCount = 4;
    msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
    msQualityLevels.NumQualityLevels = 0;
    //检查后会将支持的Msaa等级赋值给msQualityLevels
    ThrowIfFailed(md3dDevice->CheckFeatureSupport(
        D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
        &msQualityLevels,
        sizeof(msQualityLevels)
    ));

    m4xMsaaQuality = msQualityLevels.NumQualityLevels;
    assert(m4xMsaaQuality > 0 && "unexpected MSAA quality level.");

    CreateCommandObjects();
    CreateSwapChain();
    CreateRtvAndDsvDescriptorHeaps();

    return true;
}

bool LittleGFXWindow::Destroy()
{
    auto succeed = LittleWindow::Destroy();
    //SAFE_RELEASE(pSwapChain);
    return succeed;
}

void LittleGFXWindow::CreateCommandObjects() {
    //命令队列描述符
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    //在Device上构建CommandQueue
    ThrowIfFailed(md3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));
    //在Device上构建CommandAllocator
    ThrowIfFailed(md3dDevice->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(mDirectCmdListAlloc.GetAddressOf())
    ));
    //在设备上构建命令列表，并把命令分配器交给他
    ThrowIfFailed(md3dDevice->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        mDirectCmdListAlloc.Get(),//Associated command allocator
        nullptr,                  //初始化PipelineStateObject
        IID_PPV_ARGS(mCommandList.GetAddressOf())
    ));

    //Start off in a closed state. This is because the first time we refer to the 
    //command list we will Reset it, and it needs to be closed before calling Reset.
    //初次构建一个命令列表后应该先关闭它
    mCommandList->Close();
}

void LittleGFXWindow::CreateSwapChain() {
    //Release the previous swapchain we will be recreating
    mSwapChain.Reset();

    DXGI_SWAP_CHAIN_DESC sd;
    sd.BufferDesc.Width = mClientWidth;
    sd.BufferDesc.Height = mClientHeight;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferDesc.Format = mBackBufferFormat;
    sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    sd.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    sd.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.Windowed = true;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    //Note: Swap chain uses queue to perform flush.
    //直接设置当前的swap chian desc给CommandQueue
    ThrowIfFailed(mdxgiFactory->CreateSwapChain(
        mCommandQueue.Get(),
        &sd,
        mSwapChain.GetAddressOf()
    ));
}

//构建实际的ZBuffer和rtv堆描述
void LittleGFXWindow::CreateRtvAndDsvDescriptorHeaps() {
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
    rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
        &rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())
    ));

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
        &dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())
    ));
}

void LittleGFXWindow::FlushCommandQueue() {
    //Advance the fence value to mark comamnds up to this fence point.
    mCurrentFence++;

    //Add an instruction to the command queue to set a new fence point.
    //Because we are on the GPU timeline,the new fence point won't be set
    //until the GPU fnishes processing all the commands prior to this Signal()
    //简单来说，栅栏
    ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));

    //Wait until the GPU has completed commands up to this fence point.
    //等待GPU到达当前的栅栏点
    if (mFence->GetCompletedValue() < mCurrentFence) {
        HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

        //Fire event when GPU hits current fence.
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, eventHandle));

        //Wait until the GPU hits current fence event is fired
        //实际的等待
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
}

void LittleGFXWindow::OnResize() {
    assert(md3dDevice);
    assert(mSwapChain);
    assert(mDirectCmdListAlloc);

    //改变前刷新一下命令队列
    FlushCommandQueue();
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    //Release the previous resources we will be recreating.
    for (int i = 0; i < SwapChainBufferCount; ++i) {
        mSwapChainBuffer[i].Reset();
    }
    mDepthStencilBuffer.Reset();

    //Resize the swap chain.
    ThrowIfFailed(mSwapChain->ResizeBuffers(
        SwapChainBufferCount,
        mClientWidth,mClientHeight,
        mBackBufferFormat,
        DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
        ));

    mCurrBackBuffer = 0;

    //DXD12那本书封装的一层DXD12_HANDLE
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < SwapChainBufferCount; ++i) {
        ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(&mSwapChainBuffer[i])));
        md3dDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
        rtvHeapHandle.Offset(1,mRtvDescriptorSize);
    }
      
    //Create the depth/stencil buffer and view 
    D3D12_RESOURCE_DESC depthStencilDesc;
    depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthStencilDesc.Alignment = 0;
    depthStencilDesc.Width = mClientWidth;
    depthStencilDesc.Height = mClientHeight;
    depthStencilDesc.DepthOrArraySize = 1;
    depthStencilDesc.MipLevels = 1;
    //Coorrection 11/12/2016: SSAO chapter requires an SRV to the depth buffer to read from
    //the depth buffer. Therefore, because we need to create two views to the same resource:
    //    1. SRV format: DXGI_FORMAT_R24_UNORM_X8_TYPELESS
    //    2. DSV Format: DXGI_FORMAT_D24_UNORM_S8_UINT
    //we need to create the depth buffer resource with a typeless format.
    depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;

    depthStencilDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    depthStencilDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optClear;
    optClear.Format = mDepthStencilFormat;
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = 0;
    ThrowIfFailed(md3dDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &depthStencilDesc,
        D3D12_RESOURCE_STATE_COMMON,
        &optClear,
        IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())
    ));

    //Create descriptor to mip level 0 of entire resource usin the format of the resource.
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Format = mDepthStencilFormat;
    dsvDesc.Texture2D.MipSlice = 0;
    md3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), &dsvDesc, DepthStencilView());

    // Transition the resource form its initial state to be used as a depth buffer.
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
        D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE
    ));

    //Execute the resize commands.
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    //Wait until resize is complete.
    FlushCommandQueue();

    mScreenViewport.TopLeftX = 0;
    mScreenViewport.TopLeftY = 0;
    mScreenViewport.Width = static_cast<float>(mClientWidth);
    mScreenViewport.Height = static_cast<float>(mClientHeight);
    mScreenViewport.MinDepth = 0.0f;
    mScreenViewport.MaxDepth = 1.0f;

    mScissorRect = { 0,0,mClientWidth,mClientHeight };
}

D3D12_CPU_DESCRIPTOR_HANDLE LittleGFXWindow::DepthStencilView() const {
    return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}

ID3D12Resource* LittleGFXWindow::CurrentBackBuffer() const
{
    return mSwapChainBuffer[mCurrBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE LittleGFXWindow::CurrentBackBufferView() const
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(
        mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
        mCurrBackBuffer,
        mRtvDescriptorSize
    );
}