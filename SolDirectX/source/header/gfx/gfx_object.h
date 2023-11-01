#pragma once
#include "../window.h"
#include <vector>
#include <dxgi1_6.h>

class LittleGFXAdapter
{
    friend class LittleGFXWindow;
    friend class LittleGFXInstance;
    friend class LittleGFXDevice;

protected:
    LittleGFXInstance* instance;
    struct IDXGIAdapter4* pDXGIAdapter;
};

class LittleGFXInstance
{
    friend class LittleGFXWindow;
    friend class LittleGFXInstance;
    friend class LittleGFXDevice;

public:
    bool Initialize(bool enableDebugLayer);
    bool Destroy();

    uint32_t GetAdapterCount() const { return adapters.size(); }
    LittleGFXAdapter* GetAdapter(uint32_t idx) { return &adapters[idx]; }

protected:
    void queryAllAdapters();
    bool debugLayerEnabled;
    struct IDXGIFactory6* pDXGIFactory = nullptr;
    std::vector<LittleGFXAdapter> adapters;
    bool foundSoftwareAdapter;
};

class LittleGFXDevice
{
    friend class LittleGFXWindow;

public:
    bool Initialize(LittleGFXAdapter* adapter);
    bool Destroy();

protected:
    LittleGFXAdapter* adapter;
    struct ID3D12Device* pD3D12Device;
    struct ID3D12CommandQueue* pD3D12Queue;
};

class LittleGFXWindow : public LittleWindow
{
    friend class LittleGFXInstance;

public:
    static LittleGFXWindow* GetWindow();
    bool Initialize(const wchar_t* title, LittleGFXDevice* device, bool enableVsync);
    bool Destroy();
    bool Get4xMsaaState()const;
    void Set4xMsaaState(bool value);

protected:
    bool vsyncEnabled = false;
    uint32_t swapchainFlags;

    virtual void Update(/*可以加入时间*/) = 0;
    virtual void Draw() = 0;

    virtual void OnMouseDown(WPARAM btnState, int x, int y) {}
    virtual void OnMouseUp(WPARAM btnState, int x, int y) {}
    virtual void OnMouseMove(WPARAM btnState, int x, int y) {}
protected:

    bool InitDirect3D();
    void CreateCommandObjects();
    void CreateSwapChain();

    void FlushCommandQueue();

    ID3D12Resource* CurrentBackBuffer() const
    {
        return mSwapChainBuffer[mCurrBackBuffer].Get();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

    void CalculateFrameStats();

    void LogAdapters();
    void LogAdapterOutputs(IDXGIAdapter* adapter);
    void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);

protected:
    static LittleGFXWindow* mWindow;

    bool m4xMsaaState = false;  //是否开启4X MSAA技术
    UINT m4xMsaaQuality = 0;

    Microsoft::WRL::ComPtr<IDXGIFactory4> mdxgiFactory;
    Microsoft::WRL::ComPtr<IDXGISwapChain> mSwapChain;
    Microsoft::WRL::ComPtr<ID3D12Device> md3dDevice;

    Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
    UINT64 mCurrentFence = 0;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;

    static const int SwapChainBufferCount = 2;
    int mCurrBackBuffer = 0;
    Microsoft::WRL::ComPtr<ID3D12Resource> mSwapChainBuffer[SwapChainBufferCount];
    Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencilBuffer;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap;

    D3D12_VIEWPORT mScreenViewport;
    D3D12_RECT mScissorRect;

    UINT mRtvDescriptorSize = 0;
    UINT mDsvDescriptorSize = 0;
    UINT mCbvSrvUavDescriptorSize = 0;

    D3D_DRIVER_TYPE md3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
    DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    int mClientWidth = 800;
    int mClientHeight = 600;
};