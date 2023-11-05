// SolDirectX.cpp: 定义应用程序的入口点。
//

#include "header/gfx/gfx_object.h"
#include <iostream>
#include "header/Window/LittleRendererWindow.h"

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
