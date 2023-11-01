#include "../../header/gfx/gfx_object.h"
#include <vector>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <iostream>
#include "../../header/d3dUtil.h"

#define LOG_ADAPTER_OUTPUTS false    //是否打印配置器的显示参数列表
#define LOG_DEVICE_FUNC_SUPPORT true //是否打印设备的功能支持

#pragma comment(lib,"d3dcompiler.lib")
// 链接到dxgi库
#pragma comment(lib, "dxgi.lib")
// 链接到d3d12
#pragma comment(lib, "d3d12.lib")

using Microsoft::WRL::ComPtr;


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
    
    return succeed;
}

bool LittleGFXWindow::InitDirect3D() {
#if defined(DEBUG) || defined(_DEBUG)
    //開啓Debug层,todo
#endif
    //IID_PPV_ARGS是用于寻找ComPtr的唯一Id的
    ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mdxgiFactory)));
    


}

bool LittleGFXWindow::Destroy()
{
    auto succeed = LittleWindow::Destroy();
    //SAFE_RELEASE(pSwapChain);
    return succeed;
}

