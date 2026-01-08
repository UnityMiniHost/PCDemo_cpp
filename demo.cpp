// demo.cpp - Demo test program

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <windows.h>
#include <iostream>
#include <string>
#include <cstring>
#include "include/IBrowsingService.h"
#include "include/IAppletManagerV3.h"
#include "include/ICoreServiceHandler.h"

// Configuration from development.json
const char* SDK_KEY = "AKIDsPmooC1zXhGdyhobjcNDS1njeGpw";
const char* SDK_SECRET = "ymyO7QeObBafhXXAM4IHLfm9h9CbBWEt";
const char* ACCESS_TOKEN = "w2uAAaGN0WOdmNiNLcKQML4JT3O-5ng2hx2Mur_1dvY01ef";
const char* USER_ID = "developer";
const char* APP_ID = "691acb162dcfadc65a0fdb77";
const char* LAUNCH_KEY = "https://minihost.tuanjie.cn/api/game/start_session?gameId=691acb162dcfadc65a0fdb77";

// Global state for launch result
static volatile bool g_launchCompleted = false;
static volatile int g_launchResultCode = -1;
static volatile bool g_shouldExit = false;
static volatile bool g_serverExited = false;
static volatile bool g_gameExited = false;  // Track if game has exited

// Forward declarations for handlers
bool OnEventHandler(const char* event_name, const char* data, size_t data_size, int task_id);

// Core service handler implementation
class DemoCoreServiceHandler : public ICoreServiceHandler {
public:
    // Simple implementations for COM interface (stack object, no ref counting needed)
    virtual INT QueryInterface(const char* type, void** ppvObject) override {
        (void)type; (void)ppvObject;
        return -1;  // Not implemented
    }
    
    virtual ULONG AddRef(void) override {
        return 1;  // No-op for stack objects
    }
    
    virtual ULONG Release(void) override {
        return 1;  // No-op for stack objects
    }
    
    virtual void OnContextInitialized() override {
        std::cout << "[CoreServiceHandler] Context initialized" << std::endl;
    }
    
    virtual void OnServiceDisconnected() override {
        std::cout << "[CoreServiceHandler] Service disconnected" << std::endl;
        g_serverExited = true;
        g_shouldExit = true;
    }
    
    virtual bool OnCommonEventHappened(const char* event_name, int32_t callback_id,
                                      const char* data, const unsigned int data_size) override {
        std::cout << "[CoreServiceHandler] Event: " << event_name << std::endl;
        std::cout << "[CoreServiceHandler] Callback ID: " << callback_id << std::endl;
        std::cout << "[CoreServiceHandler] Data size: " << data_size << std::endl;
        if (data && data_size > 0) {
            std::cout << "[CoreServiceHandler] Data: " << data << std::endl;
        }
        
        // Check for server process exit event
        if (event_name && strcmp(event_name, "cservice_event_report_process") == 0) {
            // Parse event data to check if server is exiting
            if (data && strstr(data, "\"event\":2")) {
                std::cout << "[CoreServiceHandler] Server process exiting, will exit client..." << std::endl;
                g_serverExited = true;
                g_shouldExit = true;
            }
        }
        
        // Check for game exit notification
        if (event_name && strcmp(event_name, "game_exited") == 0) {
            std::cout << "[CoreServiceHandler] Game process exited, will exit client..." << std::endl;
            g_gameExited = true;  // Mark game as exited
            g_shouldExit = true;
        }
        
        return true;
    }
};

// Launch callback
void OnLaunchComplete(const char* appId, int resultCode, const char* errorDesc) {
    std::cout << "\n[LaunchCallback] Launch finished!" << std::endl;
    std::cout << "  App ID: " << appId << std::endl;
    std::cout << "  Result Code: " << resultCode << std::endl;
    if (errorDesc && strlen(errorDesc) > 0) {
        std::cout << "  Error: " << errorDesc << std::endl;
    }
    
    g_launchCompleted = true;
    g_launchResultCode = resultCode;
    
    // If launch failed, signal to exit
    if (resultCode != 0) {
        std::cerr << "[LaunchCallback] Launch failed, will exit..." << std::endl;
        g_shouldExit = true;
    }
}

// Event handler - called when game sends an event
bool OnEventHandler(const char* event_name, const char* data, size_t data_size, int task_id) {
    std::cout << "\n[EventHandler] Event received!" << std::endl;
    std::cout << "  Event Name: " << event_name << std::endl;
    std::cout << "  Task ID: " << task_id << std::endl;
    std::cout << "  Data Size: " << data_size << std::endl;
    if (data && data_size > 0) {
        std::cout << "  Data: " << std::string(data, data_size) << std::endl;
    }
    
    // Exit demo immediately when game exits (notification is forwarded from AppletManager)
    if (event_name && strcmp(event_name, "game_exited") == 0) {
        std::cout << "[EventHandler] Game exited, will exit demo..." << std::endl;
        g_gameExited = true;
        g_shouldExit = true;
    }

    // Return true to indicate we handled the event
    return true;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "WebGLHost Native Demo Test" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    // Load DLL
    HMODULE hDll = LoadLibraryA("host\\webglhost_export.dll");
    if (!hDll) {
        std::cerr << "Failed to load DLL: " << GetLastError() << std::endl;
        std::cerr << "Make sure webglhost_export.dll is in the same directory or PATH" << std::endl;
        return 1;
    }
    
    std::cout << "[OK] DLL loaded successfully\n" << std::endl;
    
    // Get function pointer
    typedef IBrowsingService* (*GetBrowsingServiceFunc)();
    GetBrowsingServiceFunc getBrowsingService = 
        (GetBrowsingServiceFunc)GetProcAddress(hDll, "GetBrowsingService");
    
    if (!getBrowsingService) {
        std::cerr << "Failed to get GetBrowsingService function" << std::endl;
        FreeLibrary(hDll);
        return 1;
    }
    
    // Create service
    IBrowsingService* service = getBrowsingService();
    if (!service) {
        std::cerr << "Failed to create BrowsingService" << std::endl;
        FreeLibrary(hDll);
        return 1;
    }
    
    std::cout << "[OK] BrowsingService created\n" << std::endl;
    
    // Initialize with real SDK credentials
    std::cout << "Initializing browsing core..." << std::endl;
    std::cout << "  SDK Key: " << SDK_KEY << std::endl;
    
    // Build config JSON
    char config[1024];
    snprintf(config, sizeof(config), R"({
        "sdkKey": "%s",
        "sdkSecret": "%s",
        "accessToken": "%s",
        "debug": true,
        "logLevel": "debug"
    })", SDK_KEY, SDK_SECRET, ACCESS_TOKEN);
    
    // Runtime path - use unpacked version
    const char* runtimePath = "runtime\\webglhost-runtime.exe";
    
    // Create handler
    DemoCoreServiceHandler handler;
    
    int result = service->InitilizeBrowsingCore(config, runtimePath, &handler);
    
    if (result != 0) {
        std::cerr << "[FAIL] Initialization failed with code: " << result << std::endl;
        service->Release();
        FreeLibrary(hDll);
        return 1;
    }
    
    std::cout << "[OK] Browsing core initialized\n" << std::endl;
    
    // Get AppletManager
    std::cout << "Getting AppletManager..." << std::endl;
    IAppletManagerV3* appletManager = nullptr;
    result = service->QueryInterface("IAppletManagerV3", (void**)&appletManager);
    
    if (result != 0 || !appletManager) {
        std::cerr << "[FAIL] Failed to get AppletManager" << std::endl;
        service->UninitializeBrowsingCore();
        service->Release();
        FreeLibrary(hDll);
        return 1;
    }
    
    std::cout << "[OK] AppletManager obtained\n" << std::endl;
    
    // Set Event handler (optional - for handling custom events from game)
    std::cout << "Setting Event handler..." << std::endl;
    appletManager->SetAppletEventHandler(OnEventHandler);
    std::cout << "[OK] Event handler set\n" << std::endl;
    
    // Prepare launch config with real credentials
    std::cout << "Launching applet..." << std::endl;
    std::cout << "  App ID: " << APP_ID << std::endl;
    std::cout << "  Launch Key: " << LAUNCH_KEY << std::endl;
    
    char launchConfig[2048];
    snprintf(launchConfig, sizeof(launchConfig), R"({
        "launchKey": "%s",
        "accessToken": "%s",
        "userId": "%s",
        "debug": true,
        "mute": false,
        "transparent": true,
        "GAME_HOST_BASE_URL": "https://minihost.tuanjie.cn",
        "API_VERSION": "v1",
        "APP_VERSION": "1.0.0"
    })", LAUNCH_KEY, ACCESS_TOKEN, USER_ID);
    
    appletManager->LaunchApplet(
        APP_ID, 
        launchConfig, 
        strlen(launchConfig),
        OnLaunchComplete
    );
    
    std::cout << "[OK] Launch request sent\n" << std::endl;
    
    // Wait for launch callback with timeout
    std::cout << "Waiting for launch callback..." << std::endl;
    int launchWaitTime = 0;
    const int launchTimeout = 30000; // 30 seconds timeout for launch
    const int checkInterval = 500;   // Check every 500ms
    
    while (!g_launchCompleted && launchWaitTime < launchTimeout) {
        Sleep(checkInterval);
        launchWaitTime += checkInterval;
        
        // Check if should exit due to error
        if (g_shouldExit) {
            std::cerr << "\n[ERROR] Launch failed, exiting..." << std::endl;
            goto cleanup;
        }
    }
    
    if (!g_launchCompleted) {
        std::cerr << "\n[ERROR] Launch timeout after " << launchTimeout / 1000 << " seconds" << std::endl;
        goto cleanup;
    }
    
    if (g_launchResultCode != 0) {
        std::cerr << "\n[ERROR] Launch failed with code: " << g_launchResultCode << std::endl;
        goto cleanup;
    }
    
    std::cout << "[OK] Game launched successfully\n" << std::endl;
    
    // Wait for game to run, monitoring server status via handler
    std::cout << "Game is running. Waiting for server events..." << std::endl;
    std::cout << "Client will exit when:" << std::endl;
    std::cout << "  1. Server process exits (reported via handler)" << std::endl;
    std::cout << "  2. Press Ctrl+C to exit manually\n" << std::endl;
    
    int monitorCount = 0;
    
    while (!g_shouldExit) {
        Sleep(checkInterval);
        monitorCount += checkInterval;
        
        // Check if server exited via handler callback
        if (g_serverExited) {
            std::cout << "\n[INFO] Server exited (reported via handler), client will exit now" << std::endl;
            break;
        }
        
        // Print status every 10 seconds
        if (monitorCount % 10000 == 0) {
            std::cout << "  Game running for " << monitorCount / 1000 << " seconds..." << std::endl;
        }
    }
    
cleanup:
    // Close applet if it was launched and hasn't exited yet
    if (g_launchCompleted && g_launchResultCode == 0 && !g_gameExited) {
        std::cout << "\nClosing applet..." << std::endl;
        appletManager->CloseApplet(APP_ID);
        std::cout << "[OK] Close applet request sent" << std::endl;
    } else if (g_gameExited) {
        std::cout << "\nGame already exited, skipping CloseApplet..." << std::endl;
    }
    
    // Cleanup resources in proper order
    std::cout << "\nCleaning up resources..." << std::endl;
    
    // 1. Release AppletManager
    if (appletManager) {
        std::cout << "  Releasing AppletManager..." << std::endl;
        appletManager->Release();
        appletManager = nullptr;
    }
    
    // 2. Uninitialize BrowsingService (this will send shutdown to server)
    if (service) {
        std::cout << "  Uninitializing BrowsingService..." << std::endl;
        service->UninitializeBrowsingCore();
        
        // 3. Release BrowsingService
        std::cout << "  Releasing BrowsingService..." << std::endl;
        service->Release();
        service = nullptr;
    }
    
    // 4. Unload DLL
    if (hDll) {
        std::cout << "  Unloading DLL..." << std::endl;
        FreeLibrary(hDll);
        hDll = nullptr;
    }
    
    std::cout << "[OK] All resources cleaned up" << std::endl;
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "Test completed!" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}

