// SolDirectX.cpp: 定义应用程序的入口点。
//

#include "header/gfx/gfx_object.h"

class LittleRendererWindow final : public LittleGFXWindow
{
public:
	void Run() override
	{
		MSG msg = { 0 };
		while (msg.message != WM_QUIT) {
			//处理系统消息
			if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			//在空闲时进行我们自己的逻辑
			else {
				//暂时什么都不做
				//Sleep 1~2ms,避免被while独占
				Sleep(1);
			}
		}
		//如果收到了WM_QUIT消息,直接退出函数
		return;
	}
};

int main()
{
	//创建并初始化实例
	auto instance = LittleFactory::Create<LittleGFXInstance>(true);
	auto device = LittleFactory::Create<LittleGFXDevice>(instance->GetAdapter(0));
	//创建并初始化窗口类
	auto window = LittleFactory::Create<LittleRendererWindow>(L"LittleMaster", device, true);
	//运行窗口类的循环
	window->Run();
	// 现在窗口已经关闭，我们清理窗口类
	LittleFactory::Destroy(window);
	// 清理实例
	LittleFactory::Destroy(device);
	LittleFactory::Destroy(instance);

	return 0;
}
