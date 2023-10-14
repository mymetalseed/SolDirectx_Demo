# CMakeList.txt: SolDirectX 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 3.8)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

project ("SolDirectX")

# 我们使用Unicode字符编码
add_definitions(-D "UNICODE")
add_definitions(-D "_UNICODE")

# 获取程序的源文件和头文件
file(GLOB_RECURSE src source/*.cxx source/*.c source/*.cc source/*.cpp)
file(GLOB_RECURSE headers source/*.hpp source/*.h source/*.hh)



# 将目标链接到windows的一些API上
set(PLATFORM_FRAMEWORKS psapi user32 advapi32 iphlpapi userenv ws2_32)

# 添加程序目标
add_executable(SolDirectX ${src} ${headers})