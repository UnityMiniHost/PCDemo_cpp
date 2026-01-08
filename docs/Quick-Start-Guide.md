# WebGLHost Native SDK - 快速开始指南

## 简介

WebGLHost Native SDK 允许您在 Windows 桌面应用中集成和运行 WebGL 游戏。本指南将帮助您快速上手。

## 系统要求

- **操作系统**: Windows 10/11 (64位)
- **开发工具**: Visual Studio 2019 或更高版本
- **C++ 标准**: C++17
- **CMake**: 3.15 或更高版本

## 目录结构

```
demo/
├── docs/                          # 文档目录
│   ├── Quick-Start-Guide.md       # 本文档
│   └── API-Integration-Guide.md   # 详细 API 文档
├── scripts/                       # 构建脚本
│   ├── build.bat                  # 构建 demo.exe
│   ├── run.bat                    # 运行测试
│   ├── build-and-run.bat          # 一键构建并运行
│   ├── clean.bat                  # 清理构建产物
│   ├── compress_split.ps1         # 分片压缩工具
│   ├── decompress_split.ps1       # 分片解压工具
│   └── USAGE.md                   # 脚本使用说明
├── include/                       # SDK 头文件
│   ├── IBrowsingService.h         # 浏览服务接口
│   ├── IAppletManagerV3.h         # 小程序管理接口
│   ├── ICoreServiceHandler.h      # 事件处理接口
│   └── WIUnknown.h                # 基础接口
├── native_sdk/                    # 压缩的 SDK 文件（供应商提供）
│   ├── host.zip                   # SDK 核心 DLL（压缩）
│   ├── runtime.z01                # 运行时文件（分片压缩包 1）
│   ├── runtime.z02                # 运行时文件（分片压缩包 2）
│   ├── runtime.z03                # 运行时文件（分片压缩包 3）
│   └── runtime.z04                # 运行时文件（分片压缩包 4）
├── host/                          # DLL 目录（自动解压）
│   └── webglhost_export.dll       # SDK 核心 DLL
├── runtime/                       # 运行时环境（自动解压）
│   └── webglhost-runtime.exe      # 运行时可执行文件
├── demo.cpp                       # 示例源码
├── CMakeLists.txt                 # CMake 配置
└── README.md                      # 项目说明
```

**注意**: `host/` 和 `runtime/` 目录会在首次运行构建或运行脚本时自动从 `native_sdk/` 解压。

## 快速开始

### 方式一：运行示例程序（推荐）

如果您想先体验 SDK 的功能，可以直接运行我们提供的示例程序：

```bat
# 进入 demo 目录
cd demo

# 一键构建并运行
scripts\build-and-run.bat
```

这将：
1. 编译 `demo.cpp` 生成 `demo.exe`
2. 自动运行示例程序
3. 启动一个测试游戏

### 方式二：集成到您的项目

#### 步骤 1: 复制必要文件

将以下文件/目录复制到您的项目中：

```
your-project/
├── include/              # 复制整个 include 目录
├── native_sdk/           # 复制整个 native_sdk 目录（压缩的 SDK 文件）
├── scripts/              # 复制 decompress_split.ps1 脚本
├── host/                 # 运行时自动解压生成
└── runtime/              # 运行时自动解压生成
```

**注意**: `host/` 和 `runtime/` 目录会在首次运行时自动从 `native_sdk/` 解压，无需手动解压。

#### 步骤 2: 配置项目

在您的 CMakeLists.txt 或项目配置中：

```cmake
# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)

# 添加头文件路径
include_directories(${CMAKE_SOURCE_DIR}/include)

# 链接 Windows Sockets 库
target_link_libraries(your_app ws2_32)
```

#### 步骤 3: 编写代码

参考 `demo.cpp` 或查看下面的最小示例。

## 最小示例

以下是一个最简单的集成示例：

```cpp
#include <windows.h>
#include <iostream>
#include "IBrowsingService.h"
#include "IAppletManagerV3.h"
#include "ICoreServiceHandler.h"

// 配置信息（请替换为您的实际配置）
const char* SDK_KEY = "your-sdk-key";
const char* SDK_SECRET = "your-sdk-secret";
const char* ACCESS_TOKEN = "your-access-token";
const char* APP_ID = "your-app-id";
const char* LAUNCH_KEY = "your-launch-url";

// 全局状态
volatile bool g_launchCompleted = false;
volatile int g_launchResultCode = -1;
volatile bool g_shouldExit = false;

// 事件处理器
class MyHandler : public ICoreServiceHandler {
public:
    INT QueryInterface(const char* type, void** ppvObject) override { return 0; }
    ULONG AddRef(void) override { return 1; }
    ULONG Release(void) override { return 1; }
    
    void OnContextInitialized() override {
        std::cout << "初始化完成" << std::endl;
    }
    
    void OnServiceDisconnected() override {
        std::cout << "服务断开" << std::endl;
        g_shouldExit = true;
    }
    
    bool OnCommonEventHappened(const char* event_name, int32_t callback_id,
                               const char* data, const unsigned int data_size) override {
        std::cout << "事件: " << event_name << std::endl;
        
        if (strcmp(event_name, "game_exited") == 0) {
            g_shouldExit = true;
        }
        
        return true;
    }
};

// 启动回调
void OnLaunchComplete(const char* appId, int resultCode, const char* errorDesc) {
    std::cout << "启动完成，结果码: " << resultCode << std::endl;
    
    g_launchCompleted = true;
    g_launchResultCode = resultCode;
    
    if (resultCode != 0) {
        std::cerr << "启动失败: " << errorDesc << std::endl;
        g_shouldExit = true;
    }
}

int main() {
    // 1. 加载 DLL
    HMODULE hDll = LoadLibraryA("host\\webglhost_export.dll");
    if (!hDll) {
        std::cerr << "加载 DLL 失败" << std::endl;
        return 1;
    }
    
    // 2. 获取服务
    typedef IBrowsingService* (*GetBrowsingServiceFunc)();
    auto getBrowsingService = (GetBrowsingServiceFunc)GetProcAddress(hDll, "GetBrowsingService");
    IBrowsingService* service = getBrowsingService();
    
    // 3. 初始化
    char config[1024];
    snprintf(config, sizeof(config), R"({
        "sdkKey": "%s",
        "sdkSecret": "%s",
        "accessToken": "%s",
        "debug": true
    })", SDK_KEY, SDK_SECRET, ACCESS_TOKEN);
    
    MyHandler handler;
    service->InitilizeBrowsingCore(config, "runtime\\webglhost-runtime.exe", &handler);
    
    // 4. 获取小程序管理器
    IAppletManagerV3* appletManager = nullptr;
    service->QueryInterface("IAppletManagerV3", (void**)&appletManager);
    
    // 5. 启动游戏
    char launchConfig[2048];
    snprintf(launchConfig, sizeof(launchConfig), R"({
        "launchKey": "%s",
        "accessToken": "%s",
        "userId": "user-001"
    })", LAUNCH_KEY, ACCESS_TOKEN);
    
    appletManager->LaunchApplet(APP_ID, launchConfig, strlen(launchConfig), OnLaunchComplete);
    
    // 6. 等待启动完成
    while (!g_launchCompleted && !g_shouldExit) {
        Sleep(100);
    }
    
    if (g_launchResultCode == 0) {
        std::cout << "游戏启动成功！" << std::endl;
        
        // 7. 等待游戏运行
        while (!g_shouldExit) {
            Sleep(100);
        }
    }
    
    // 8. 清理资源
    if (g_launchCompleted && g_launchResultCode == 0) {
        appletManager->CloseApplet(APP_ID);
    }
    
    appletManager->Release();
    service->UninitializeBrowsingCore();
    service->Release();
    FreeLibrary(hDll);
    
    return 0;
}
```

## 核心概念

### 1. IBrowsingService

浏览服务是 SDK 的入口点，负责：
- 初始化 SDK 环境
- 管理运行时进程
- 提供其他服务接口

**关键方法**:
- `InitilizeBrowsingCore()` - 初始化服务
- `UninitializeBrowsingCore()` - 清理服务
- `QueryInterface()` - 获取其他接口

### 2. IAppletManagerV3

小程序管理器负责游戏的生命周期：
- 启动游戏
- 关闭游戏
- 处理游戏事件

**关键方法**:
- `LaunchApplet()` - 启动游戏
- `CloseApplet()` - 关闭游戏
- `SetAppletEventHandler()` - 设置事件处理器

### 3. ICoreServiceHandler

事件处理器接收系统事件：
- 初始化完成通知
- 服务断开通知
- 游戏退出通知

**关键方法**:
- `OnContextInitialized()` - 初始化完成
- `OnServiceDisconnected()` - 服务断开
- `OnCommonEventHappened()` - 通用事件

## 配置说明

### SDK 配置

初始化时需要提供 JSON 格式的配置：

```json
{
    "sdkKey": "您的 SDK Key",
    "sdkSecret": "您的 SDK Secret",
    "accessToken": "访问令牌",
    "debug": true,          // 是否开启调试模式
    "logLevel": "debug"     // 日志级别: debug, info, warn, error
}
```

### 启动配置

启动游戏时需要提供：

```json
{
    "launchKey": "游戏启动 URL",
    "accessToken": "访问令牌",
    "userId": "用户 ID",
    "debug": true,          // 是否开启调试
    "mute": false,          // 是否静音
    "transparent": true     // 窗口是否透明
}
```

## 常见问题

### Q1: DLL 加载失败

**错误**: `Failed to load DLL: 126`

**解决方案**:
1. 确认 `host/webglhost_export.dll` 存在
2. 确认 DLL 与程序架构匹配（都是 x64）
3. 检查是否缺少 Visual C++ Redistributable

### Q2: 初始化失败

**错误**: `Initialization failed: -2`

**解决方案**:
1. 检查配置 JSON 格式是否正确
2. 确认 SDK Key 和 Secret 有效
3. 确认 runtime 路径正确

### Q3: 游戏启动失败

**错误**: Launch callback 返回非 0 结果码

**解决方案**:
1. 检查 App ID 是否正确
2. 确认 Launch Key 有效
3. 检查网络连接
4. 查看 `logs/` 目录中的日志文件

### Q4: 找不到 runtime

**错误**: `Runtime not found`

**解决方案**:
1. 构建和运行脚本会自动从 `native_sdk/` 解压 runtime
2. 如果自动解压失败，请检查 `native_sdk/runtime.z01~z04` 是否完整
3. 手动解压命令：
   ```bat
   powershell -ExecutionPolicy Bypass -File "scripts\decompress_split.ps1" -SourceDir "native_sdk" -OutputDir "." -ArchiveName "runtime"
   ```

### Q5: 解压失败

**错误**: `Failed to extract host/runtime directory`

**解决方案**:
1. 确认 `native_sdk/` 目录中所有压缩文件完整且未损坏
2. 检查 PowerShell 执行策略是否允许脚本运行
3. 确认磁盘空间充足（runtime 解压后约 150MB）
4. 尝试手动解压验证文件完整性

## 调试技巧

### 1. 启用调试日志

在配置中设置：

```json
{
    "debug": true,
    "logLevel": "debug"
}
```

### 2. 查看日志文件

日志文件位于 `logs/` 目录，文件名格式：`server-YYYY-MM-DDTHH-MM-SS.log`

### 3. 检查事件

在 `OnCommonEventHappened` 中打印所有事件：

```cpp
bool OnCommonEventHappened(const char* event_name, int32_t callback_id,
                           const char* data, const unsigned int data_size) override {
    std::cout << "[事件] " << event_name << std::endl;
    if (data && data_size > 0) {
        std::cout << "[数据] " << std::string(data, data_size) << std::endl;
    }
    return true;
}
```

## 下一步

- 📖 查看 [API Integration Guide](./API-Integration-Guide.md) 了解详细 API 文档
- 📝 查看 `demo.cpp` 了解完整示例
- 🔧 查看 `scripts/USAGE.md` 了解构建脚本使用方法
- 📂 查看 `logs/` 目录中的运行日志进行调试

## 技术支持

如有问题，请：
1. 查看日志文件 (`logs/` 目录)
2. 参考完整 API 文档
3. 联系技术支持

---

**版本**: 1.0.0  
**最后更新**: 2025-12-30

