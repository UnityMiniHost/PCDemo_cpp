// demo.cpp - Demo test program
// Cross-platform support for Windows and macOS

#include <iostream>
#include <string>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>

// Platform-specific includes
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <windows.h>
    #include <winhttp.h>
    #pragma comment(lib, "winhttp.lib")
#else
    #include <dlfcn.h>
    #include <unistd.h>
    #include <signal.h>
    #include <curl/curl.h>
#endif

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
static volatile bool g_gameExited = false;

// Global applet manager pointer for use in handlers
static IAppletManagerV3* g_appletManager = nullptr;

// Global ad data storage for showRewardAd
struct AdData {
    std::string id;
    std::string adType;
    std::string name;
    std::string image;
    std::string url;
    std::string color;
    std::string createdTime;
    int duration;
    bool isShowLogo;
    bool archived;
    bool loaded;
    
    AdData() : duration(10), isShowLogo(false), archived(false), loaded(false) {}
};
static AdData g_adData;

// Forward declarations for handlers
bool OnEventHandler(const char* event_name, const char* data, size_t data_size, int task_id);
bool OnJsApiHandler(const char* app_id, const char* api_name, const char* data, size_t data_size, int task_id);

// Helper function: URL encode a string
std::string UrlEncode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }

        // Any other characters are percent-encoded
        escaped << std::uppercase;
        escaped << '%' << std::setw(2) << int(static_cast<unsigned char>(c));
        escaped << std::nouppercase;
    }

    return escaped.str();
}

// Helper function: Unescape JSON string (handle \" -> ")
std::string UnescapeJsonString(const std::string& escaped) {
    std::string result;
    result.reserve(escaped.length());
    
    for (size_t i = 0; i < escaped.length(); ++i) {
        if (escaped[i] == '\\' && i + 1 < escaped.length()) {
            char next = escaped[i + 1];
            if (next == '\"' || next == '\\' || next == '/') {
                result += next;
                ++i; // Skip the escaped character
            } else if (next == 'n') {
                result += '\n';
                ++i;
            } else if (next == 'r') {
                result += '\r';
                ++i;
            } else if (next == 't') {
                result += '\t';
                ++i;
            } else {
                result += escaped[i];
            }
        } else {
            result += escaped[i];
        }
    }
    
    return result;
}

// Helper function: Extract JSON value by key (simple parser for string values)
std::string ExtractJsonStringValue(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\":\"";
    size_t startPos = json.find(searchKey);
    if (startPos == std::string::npos) {
        return "";
    }
    
    startPos += searchKey.length();
    size_t endPos = json.find("\"", startPos);
    if (endPos == std::string::npos) {
        return "";
    }
    
    return json.substr(startPos, endPos - startPos);
}

// Helper function: Extract integer value from JSON
int ExtractJsonIntValue(const std::string& json, const std::string& key, int defaultValue = 0) {
    std::string searchKey = "\"" + key + "\":";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) {
        return defaultValue;
    }
    
    pos += searchKey.length();
    std::string valueStr;
    while (pos < json.length() && (isdigit(json[pos]) || json[pos] == ' ' || json[pos] == '-')) {
        if (isdigit(json[pos]) || json[pos] == '-') {
            valueStr += json[pos];
        }
        pos++;
    }
    
    if (!valueStr.empty()) {
        try {
            return std::stoi(valueStr);
        } catch (...) {
            return defaultValue;
        }
    }
    return defaultValue;
}

// Helper function: Extract boolean value from JSON
bool ExtractJsonBoolValue(const std::string& json, const std::string& key, bool defaultValue = false) {
    std::string truePattern = "\"" + key + "\":true";
    if (json.find(truePattern) != std::string::npos) {
        return true;
    }
    std::string falsePattern = "\"" + key + "\":false";
    if (json.find(falsePattern) != std::string::npos) {
        return false;
    }
    return defaultValue;
}

// Helper function: Escape string for JavaScript embedding
std::string EscapeForJs(const std::string& str) {
    std::string result;
    for (char c : str) {
        if (c == '\'') result += "\\'";
        else if (c == '\\') result += "\\\\";
        else if (c == '\n') result += "\\n";
        else if (c == '\r') result += "\\r";
        else result += c;
    }
    return result;
}

// Helper function: Escape string for JSON embedding
std::string EscapeForJson(const std::string& str) {
    std::string result;
    for (char c : str) {
        if (c == '"') result += "\\\"";
        else if (c == '\\') result += "\\\\";
        else if (c == '\n') result += "\\n";
        else if (c == '\r') result += "\\r";
        else if (c == '\t') result += "\\t";
        else result += c;
    }
    return result;
}

// Platform-specific HTTP implementation
#ifdef _WIN32
// Windows HTTP implementation using WinHTTP
std::string HttpGet(const std::wstring& url) {
    std::string response;
    HINTERNET hSession = NULL;
    HINTERNET hConnect = NULL;
    HINTERNET hRequest = NULL;
    
    try {
        // Parse URL
        URL_COMPONENTS urlComp;
        ZeroMemory(&urlComp, sizeof(urlComp));
        urlComp.dwStructSize = sizeof(urlComp);
        
        wchar_t hostname[256];
        wchar_t urlPath[1024];
        urlComp.lpszHostName = hostname;
        urlComp.dwHostNameLength = sizeof(hostname) / sizeof(wchar_t);
        urlComp.lpszUrlPath = urlPath;
        urlComp.dwUrlPathLength = sizeof(urlPath) / sizeof(wchar_t);
        
        if (!WinHttpCrackUrl(url.c_str(), (DWORD)url.length(), 0, &urlComp)) {
            throw std::runtime_error("Failed to parse URL");
        }
        
        // Initialize WinHTTP
        hSession = WinHttpOpen(L"WebGLHost Demo/1.0",
                              WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                              WINHTTP_NO_PROXY_NAME,
                              WINHTTP_NO_PROXY_BYPASS, 0);
        
        if (!hSession) {
            throw std::runtime_error("WinHttpOpen failed");
        }
        
        // Connect
        hConnect = WinHttpConnect(hSession, hostname, urlComp.nPort, 0);
        if (!hConnect) {
            throw std::runtime_error("WinHttpConnect failed");
        }
        
        // Open request
        DWORD dwFlags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
        hRequest = WinHttpOpenRequest(hConnect, L"GET", urlPath,
                                     NULL, WINHTTP_NO_REFERER,
                                     WINHTTP_DEFAULT_ACCEPT_TYPES,
                                     dwFlags);
        
        if (!hRequest) {
            throw std::runtime_error("WinHttpOpenRequest failed");
        }
        
        // Send request
        if (!WinHttpSendRequest(hRequest,
                               WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                               WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
            throw std::runtime_error("WinHttpSendRequest failed");
        }
        
        // Receive response
        if (!WinHttpReceiveResponse(hRequest, NULL)) {
            throw std::runtime_error("WinHttpReceiveResponse failed");
        }
        
        // Read data
        DWORD dwSize = 0;
        DWORD dwDownloaded = 0;
        char buffer[4096];
        
        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) {
                throw std::runtime_error("WinHttpQueryDataAvailable failed");
            }
            
            if (dwSize == 0) {
                break;
            }
            
            if (dwSize > sizeof(buffer)) {
                dwSize = sizeof(buffer);
            }
            
            ZeroMemory(buffer, sizeof(buffer));
            
            if (!WinHttpReadData(hRequest, buffer, dwSize, &dwDownloaded)) {
                throw std::runtime_error("WinHttpReadData failed");
            }
            
            response.append(buffer, dwDownloaded);
            
        } while (dwSize > 0);
        
    } catch (const std::exception& e) {
        std::cerr << "[HttpGet] Error: " << e.what() << std::endl;
        
        // Cleanup on error
        if (hRequest) WinHttpCloseHandle(hRequest);
        if (hConnect) WinHttpCloseHandle(hConnect);
        if (hSession) WinHttpCloseHandle(hSession);
        
        throw;
    }
    
    // Cleanup
    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);
    
    return response;
}

// Overload for std::string URL on Windows
std::string HttpGet(const std::string& url) {
    std::wstring wurl(url.begin(), url.end());
    return HttpGet(wurl);
}

#else
// macOS/POSIX HTTP implementation using libcurl

// Callback function for curl to write response data
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    std::string* response = static_cast<std::string*>(userp);
    response->append(static_cast<char*>(contents), realsize);
    return realsize;
}

std::string HttpGet(const std::string& url) {
    std::string response;
    CURL* curl = curl_easy_init();
    
    if (!curl) {
        throw std::runtime_error("Failed to initialize curl");
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "WebGLHost Demo/1.0");
    
    // For HTTPS
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        curl_easy_cleanup(curl);
        throw std::runtime_error(std::string("curl_easy_perform() failed: ") + curl_easy_strerror(res));
    }
    
    curl_easy_cleanup(curl);
    return response;
}

#endif

// Perform Unity Connect authentication
std::string PerformUnityConnectAuth(const std::string& accessToken) {
    std::cout << "[Auth] Performing Unity Connect authentication..." << std::endl;
    
    try {
        std::string encodedToken = UrlEncode(accessToken);
        std::string urlStr = "https://example.com/minihost?token=" + encodedToken;
        
        std::cout << "[Auth] Request URL: " << urlStr << std::endl;
        
        std::string responseBody = HttpGet(urlStr);
        
        std::cout << "[Auth] Response: " << responseBody << std::endl;
        
        // Extract LSToken from response
        std::string lsToken = ExtractJsonStringValue(responseBody, "LSToken");
        
        if (lsToken.empty()) {
            throw std::runtime_error("LSToken not found in auth response");
        }
        
        std::cout << "[Auth] LSToken obtained: " << lsToken << std::endl;
        
        return lsToken;
        
    } catch (const std::exception& e) {
        std::cerr << "[Auth] Authentication failed: " << e.what() << std::endl;
        throw;
    }
}

// Handler: TJLoginHost - Unity Connect authentication
bool HandleTJLoginHost(const char* app_id, const std::string& dataStr, int task_id) {
    (void)app_id;
    std::cout << "[JsApiHandler] Handling TJLoginHost authentication..." << std::endl;
    
    try {
        std::string unescapedData = UnescapeJsonString(dataStr);
        std::cout << "[JsApiHandler] Unescaped data: " << unescapedData << std::endl;
        
        std::string accessToken = ExtractJsonStringValue(unescapedData, "accessToken");
        
        if (accessToken.empty()) {
            std::cerr << "[JsApiHandler] accessToken not found in request data" << std::endl;
            std::string errorResponse = R"({"success":false,"errorCode":"MISSING_TOKEN","error":"accessToken is required"})";
            if (g_appletManager) {
                g_appletManager->SendJsApiOrEventResponse(task_id, errorResponse.c_str(), errorResponse.length());
            }
            return false;
        }
        
        std::cout << "[JsApiHandler] Using accessToken for authentication" << std::endl;
        std::string lsToken = PerformUnityConnectAuth(accessToken);
        
        std::ostringstream responseOss;
        responseOss << R"({"success":true,"result":{"code":")" << lsToken << R"("}})";
        std::string successResponse = responseOss.str();
        
        std::cout << "[JsApiHandler] Authentication successful, sending response..." << std::endl;
        if (g_appletManager) {
            g_appletManager->SendJsApiOrEventResponse(task_id, successResponse.c_str(), successResponse.length());
        }
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[JsApiHandler] Authentication error: " << e.what() << std::endl;
        std::ostringstream errorOss;
        errorOss << R"({"success":false,"errorCode":"API_ERROR","error":")" << e.what() << R"("})";
        std::string errorResponse = errorOss.str();
        if (g_appletManager) {
            g_appletManager->SendJsApiOrEventResponse(task_id, errorResponse.c_str(), errorResponse.length());
        }
        return false;
    }
}

// Handler: loadRewardAd - Load reward ad data from API
bool HandleLoadRewardAd(const char* app_id, const std::string& dataStr, int task_id) {
    (void)app_id;  // Unused parameter
    std::cout << "[JsApiHandler] Handling loadRewardAd..." << std::endl;
    
    try {
        std::string unescapedData = UnescapeJsonString(dataStr);
        std::cout << "[JsApiHandler] Unescaped data: " << unescapedData << std::endl;
        
        std::string adUnitId = ExtractJsonStringValue(unescapedData, "adUnitId");
        std::cout << "[JsApiHandler] Ad unit ID: " << (adUnitId.empty() ? "(default)" : adUnitId) << std::endl;
        
        // Fetch ad data from Unity Connect API
        std::cout << "[JsApiHandler] Loading reward ad from Unity Connect API..." << std::endl;
        std::string adApiResponse = HttpGet("https://connect.unity.cn/api/connect-game/ads");
        std::cout << "[JsApiHandler] Ad API response received, length: " << adApiResponse.length() << std::endl;
        
        // Parse ad data
        std::string adId = ExtractJsonStringValue(adApiResponse, "id");
        std::string adType = ExtractJsonStringValue(adApiResponse, "adType");
        std::string adName = ExtractJsonStringValue(adApiResponse, "name");
        std::string adImage = ExtractJsonStringValue(adApiResponse, "image");
        std::string adUrl = ExtractJsonStringValue(adApiResponse, "url");
        std::string adColor = ExtractJsonStringValue(adApiResponse, "color");
        std::string createdTime = ExtractJsonStringValue(adApiResponse, "createdTime");
        int duration = ExtractJsonIntValue(adApiResponse, "duration", 10);
        bool isShowLogo = ExtractJsonBoolValue(adApiResponse, "isShowLogo", false);
        bool archived = ExtractJsonBoolValue(adApiResponse, "archived", false);
        
        std::cout << "[JsApiHandler] Parsed ad data - id: " << adId 
                  << ", adType: " << adType 
                  << ", duration: " << duration << std::endl;
        
        // Store ad data globally
        g_adData.id = adId;
        g_adData.adType = adType;
        g_adData.name = adName;
        g_adData.image = adImage;
        g_adData.url = adUrl;
        g_adData.color = adColor;
        g_adData.createdTime = createdTime;
        g_adData.duration = duration;
        g_adData.isShowLogo = isShowLogo;
        g_adData.archived = archived;
        g_adData.loaded = true;
        std::cout << "[JsApiHandler] Ad data stored globally for showRewardAd" << std::endl;
        
        // Build success response (avoid Chinese characters in response, they work fine in JSON)
        std::ostringstream responseOss;
        responseOss << R"({"success":true,"result":{)";
        responseOss << R"("id":")" << adId << R"(",)";
        responseOss << R"("adType":")" << adType << R"(",)";
        responseOss << R"("name":")" << adName << R"(",)";
        responseOss << R"("image":")" << adImage << R"(",)";
        responseOss << R"("duration":)" << duration << R"(,)";
        responseOss << R"("color":")" << adColor << R"(",)";
        responseOss << R"("isShowLogo":)" << (isShowLogo ? "true" : "false") << R"(,)";
        responseOss << R"("archived":)" << (archived ? "true" : "false") << R"(,)";
        responseOss << R"("url":")" << adUrl << R"(",)";
        responseOss << R"("createdTime":")" << createdTime << R"(")";
        responseOss << R"(}})";
        std::string successResponse = responseOss.str();
        
        std::cout << "[JsApiHandler] loadRewardAd successful, sending response..." << std::endl;
        if (g_appletManager) {
            g_appletManager->SendJsApiOrEventResponse(task_id, successResponse.c_str(), successResponse.length());
        }
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[JsApiHandler] loadRewardAd error: " << e.what() << std::endl;
        std::ostringstream errorOss;
        errorOss << R"({"success":false,"errorCode":"LOAD_AD_FAILED","error":")" << e.what() << R"("})";
        std::string errorResponse = errorOss.str();
        if (g_appletManager) {
            g_appletManager->SendJsApiOrEventResponse(task_id, errorResponse.c_str(), errorResponse.length());
        }
        return false;
    }
}

// Handler: showRewardAd - Display reward ad UI
bool HandleShowRewardAd(const char* app_id, const std::string& dataStr, int task_id) {
    (void)dataStr;  // Unused parameter
    std::cout << "[JsApiHandler] Handling showRewardAd..." << std::endl;
    
    try {
        if (!g_adData.loaded) {
            std::cout << "[JsApiHandler] No ad data available, please call loadRewardAd first" << std::endl;
            std::string errorResponse = R"({"success":false,"errorCode":"NO_AD_DATA","error":"No ad data available. Please call loadRewardAd first."})";
            if (g_appletManager) {
                g_appletManager->SendJsApiOrEventResponse(task_id, errorResponse.c_str(), errorResponse.length());
            }
            return false;
        }
        
        std::cout << "[JsApiHandler] Showing reward ad - id: " << g_adData.id 
                  << ", duration: " << g_adData.duration << std::endl;
        
        std::string escapedImage = EscapeForJs(g_adData.image);
        std::string escapedName = EscapeForJs(g_adData.name);
        std::string escapedUrl = g_adData.url.empty() ? "https://connect.unity.cn" : EscapeForJs(g_adData.url);
        int duration = g_adData.duration > 0 ? g_adData.duration : 10;
        
        // Build the ad UI creation script (simplified for brevity)
        std::ostringstream adScriptOss;
        adScriptOss << R"((function() {
        var renderRootContainer = document.getElementById('render-root-container');
        if (!renderRootContainer) {
          console.error('[C++ showRewardAd] render-root-container not found');
          return false;
        }
        
        var adContainer = document.createElement('div');
        adContainer.id = 'tj-reward-ad-container';
        adContainer.style.position = 'absolute';
        adContainer.style.top = '0';
        adContainer.style.left = '0';
        adContainer.style.width = '100%';
        adContainer.style.height = '100%';
        adContainer.style.backgroundColor = '#000000';
        adContainer.style.zIndex = '99999';
        adContainer.style.display = 'flex';
        adContainer.style.flexDirection = 'column';
        adContainer.style.justifyContent = 'center';
        adContainer.style.alignItems = 'center';
        
        var imageContainer = document.createElement('div');
        imageContainer.style.display = 'flex';
        imageContainer.style.justifyContent = 'center';
        imageContainer.style.alignItems = 'center';
        imageContainer.style.width = '100%';
        imageContainer.style.height = '100%';
        
        var adImage = document.createElement('img');
        adImage.src = ')" << escapedImage << R"(';
        adImage.style.borderRadius = '6px';
        adImage.style.cursor = 'pointer';
        adImage.alt = ')" << escapedName << R"(';

        function updateImageSize() {
          var containerWidth = renderRootContainer.offsetWidth;
          var containerHeight = renderRootContainer.offsetHeight;
          var isLandscape = containerWidth > containerHeight;
          if (isLandscape) {
            adImage.style.objectFit = 'contain'; 
            adImage.style.height = '100%';
            adImage.style.width = 'auto';  
          } else {
            adImage.style.objectFit = 'cover'; 
            adImage.style.width = '100%';
            adImage.style.height = '100%';
          }
        }
        updateImageSize();

        var timerContainer = document.createElement('div');
        timerContainer.style.position = 'absolute';
        timerContainer.style.top = '20px';
        timerContainer.style.right = '20px';
        timerContainer.style.color = 'white';
        timerContainer.style.fontSize = '14px';
        timerContainer.style.backgroundColor = 'rgba(0, 0, 0, 0.5)';
        timerContainer.style.padding = '8px 12px';
        timerContainer.style.borderRadius = '30px';
        timerContainer.style.zIndex = '100000';
        
        var timerText = document.createElement('span');
        timerText.id = 'tj-reward-ad-timer';
        // 广告将在 X 秒后关闭
        timerText.textContent = '\u5e7f\u544a\u5c06\u5728 )" << duration << R"( \u79d2\u540e\u5173\u95ed';
        timerContainer.appendChild(timerText);
        
        adImage.addEventListener('click', function() { 
          var adUrl = ')" << escapedUrl << R"(';
          if (typeof tj !== 'undefined' && tj.customCommand) {
            tj.customCommand("openSystemBrowser", {
              url: adUrl,
              success: function(res) { console.info("[C++ showRewardAd] openSystemBrowser success:", res); },
              fail: function(res) { console.info("[C++ showRewardAd] openSystemBrowser fail:", res); window.open(adUrl, '_blank'); }
            });
          } else {
            window.open(adUrl, '_blank');
          }
        });
        
        imageContainer.appendChild(adImage);
        adContainer.appendChild(imageContainer);
        adContainer.appendChild(timerContainer);
        renderRootContainer.appendChild(adContainer);
        
        // Start countdown timer
        var remainingTime = )" << duration << R"(;
        var countdownInterval = setInterval(function() {
          remainingTime--;
          var timerElement = document.getElementById('tj-reward-ad-timer');
          if (timerElement) {
            // 广告将在 X 秒后关闭
            timerElement.textContent = '\u5e7f\u544a\u5c06\u5728 ' + remainingTime + ' \u79d2\u540e\u5173\u95ed';
          }
          
          if (remainingTime <= 0) {
            clearInterval(countdownInterval);
            var adContainerEl = document.getElementById('tj-reward-ad-container');
            if (adContainerEl) {
              adContainerEl.remove();
            }
            if (typeof rewardedVideoCloseCallback === 'function') {
              rewardedVideoCloseCallback(true);
            }
            console.log('[C++ showRewardAd] Ad completed');
          }
        }, 1000);
        
        return true;
      })())";
        
        std::string adScript = adScriptOss.str();
        
        // Inject script via CallAppletCommand
        std::cout << "[JsApiHandler] Injecting ad UI script via CallAppletCommand..." << std::endl;
        
        std::string escapedScript = EscapeForJson(adScript);
        std::ostringstream scriptDataOss;
        scriptDataOss << R"({"script":")" << escapedScript << R"("})";
        std::string scriptData = scriptDataOss.str();
        
        if (g_appletManager) {
            g_appletManager->CallAppletCommand("runCustomScript", task_id, app_id, 
                                               scriptData.c_str(), scriptData.length());
        }
        
        // Send immediate success response
        std::ostringstream responseOss;
        responseOss << R"({"success":true,"result":{"adId":")" << g_adData.id << R"(","message":"Reward ad is showing"}})";
        std::string successResponse = responseOss.str();
        
        std::cout << "[JsApiHandler] showRewardAd UI injected, sending success response..." << std::endl;
        if (g_appletManager) {
            g_appletManager->SendJsApiOrEventResponse(task_id, successResponse.c_str(), successResponse.length());
        }
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[JsApiHandler] showRewardAd error: " << e.what() << std::endl;
        std::ostringstream errorOss;
        errorOss << R"({"success":false,"errorCode":"SHOW_AD_FAILED","error":")" << e.what() << R"("})";
        std::string errorResponse = errorOss.str();
        if (g_appletManager) {
            g_appletManager->SendJsApiOrEventResponse(task_id, errorResponse.c_str(), errorResponse.length());
        }
        return false;
    }
}

// Main JsApi handler - dispatches to specific handlers
bool OnJsApiHandler(const char* app_id, const char* api_name, const char* data, size_t data_size, int task_id) {
    std::cout << "\n[JsApiHandler] JsApi call received!" << std::endl;
    std::cout << "  App ID: " << app_id << std::endl;
    std::cout << "  API Name: " << api_name << std::endl;
    std::cout << "  Task ID: " << task_id << std::endl;
    std::cout << "  Data Size: " << data_size << std::endl;
    
    std::string dataStr = (data && data_size > 0) ? std::string(data, data_size) : "";
    if (!dataStr.empty()) {
        std::cout << "  Data: " << dataStr << std::endl;
    }
    
    // Dispatch to specific handler based on api_name
    if (api_name) {
        if (strcmp(api_name, "TJLoginHost") == 0) {
            return HandleTJLoginHost(app_id, dataStr, task_id);
        }
        if (strcmp(api_name, "loadRewardAd") == 0) {
            return HandleLoadRewardAd(app_id, dataStr, task_id);
        }
        if (strcmp(api_name, "showRewardAd") == 0) {
            return HandleShowRewardAd(app_id, dataStr, task_id);
        }
    }
    
    // API not handled - send not_handled response so server can use fallback
    std::cout << "[JsApiHandler] API not handled: " << (api_name ? api_name : "null") << std::endl;
    
    std::ostringstream responseOss;
    responseOss << R"({"success":false,"errorCode":"NOT_HANDLED","error":"API ')" 
                << (api_name ? api_name : "null") 
                << R"(' is not handled by client"})";
    std::string notHandledResponse = responseOss.str();
    
    if (g_appletManager) {
        g_appletManager->SendJsApiOrEventResponse(task_id, notHandledResponse.c_str(), notHandledResponse.length());
    }
    
    return false;
}

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

// Platform-specific library loading
#ifdef _WIN32

typedef IBrowsingService* (*GetBrowsingServiceFunc)();

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "WebGLHost Native Demo Test (Windows)" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    // Load DLL
    HMODULE hDll = LoadLibraryA("host\\webglhost_export.dll");
    if (!hDll) {
        std::cerr << "Failed to load DLL: " << GetLastError() << std::endl;
        std::cerr << "Make sure webglhost_export.dll is in the host directory" << std::endl;
        return 1;
    }
    
    std::cout << "[OK] DLL loaded successfully\n" << std::endl;
    
    GetBrowsingServiceFunc getBrowsingService = 
        (GetBrowsingServiceFunc)GetProcAddress(hDll, "GetBrowsingService");
    
    if (!getBrowsingService) {
        std::cerr << "Failed to get GetBrowsingService function" << std::endl;
        FreeLibrary(hDll);
        return 1;
    }

#else

typedef IBrowsingService* (*GetBrowsingServiceFunc)();

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "WebGLHost Native Demo Test (macOS)" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    // Initialize curl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // Load dylib
    void* hLib = dlopen("host/libwebglhost_export.dylib", RTLD_NOW | RTLD_LOCAL);
    if (!hLib) {
        std::cerr << "Failed to load dylib: " << dlerror() << std::endl;
        std::cerr << "Make sure libwebglhost_export.dylib is in the host directory" << std::endl;
        curl_global_cleanup();
        return 1;
    }
    
    std::cout << "[OK] Dylib loaded successfully\n" << std::endl;
    
    GetBrowsingServiceFunc getBrowsingService = 
        (GetBrowsingServiceFunc)dlsym(hLib, "GetBrowsingService");
    
    if (!getBrowsingService) {
        std::cerr << "Failed to get GetBrowsingService function: " << dlerror() << std::endl;
        dlclose(hLib);
        curl_global_cleanup();
        return 1;
    }

#endif

    // Common code for both platforms
    IBrowsingService* service = getBrowsingService();
    if (!service) {
        std::cerr << "Failed to create BrowsingService" << std::endl;
#ifdef _WIN32
        FreeLibrary(hDll);
#else
        dlclose(hLib);
        curl_global_cleanup();
#endif
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
    
    // Runtime path
#ifdef _WIN32
    const char* runtimePath = "runtime\\webglhost-runtime.exe";
#else
    // macOS: runtime is a .app bundle, executable is inside Contents/MacOS/
    const char* runtimePath = "runtime/webglhost-runtime.app/Contents/MacOS/webglhost-runtime";
#endif
    
    DemoCoreServiceHandler handler;
    
    int result = service->InitilizeBrowsingCore(config, runtimePath, &handler);
    
    if (result != 0) {
        std::cerr << "[FAIL] Initialization failed with code: " << result << std::endl;
        service->Release();
#ifdef _WIN32
        FreeLibrary(hDll);
#else
        dlclose(hLib);
        curl_global_cleanup();
#endif
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
#ifdef _WIN32
        FreeLibrary(hDll);
#else
        dlclose(hLib);
        curl_global_cleanup();
#endif
        return 1;
    }
    
    std::cout << "[OK] AppletManager obtained\n" << std::endl;
    
    // Store global reference for use in handlers
    g_appletManager = appletManager;
    
    // Set JsApi handler (for handling custom JsApi calls like TJLoginHost)
    std::cout << "Setting JsApi handler..." << std::endl;
    appletManager->SetJsApiHandler(OnJsApiHandler);
    std::cout << "[OK] JsApi handler set\n" << std::endl;
    
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
        std::this_thread::sleep_for(std::chrono::milliseconds(checkInterval));
        launchWaitTime += checkInterval;
        
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
    
    {
        int monitorCount = 0;
        
        while (!g_shouldExit) {
            std::this_thread::sleep_for(std::chrono::milliseconds(checkInterval));
            monitorCount += checkInterval;
            
            if (g_serverExited) {
                std::cout << "\n[INFO] Server exited (reported via handler), client will exit now" << std::endl;
                break;
            }
            
            if (monitorCount % 10000 == 0) {
                std::cout << "  Game running for " << monitorCount / 1000 << " seconds..." << std::endl;
            }
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
        g_appletManager = nullptr;
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
    
#ifdef _WIN32
    if (hDll) {
        std::cout << "  Unloading DLL..." << std::endl;
        FreeLibrary(hDll);
        hDll = nullptr;
    }
#else
    if (hLib) {
        std::cout << "  Unloading dylib..." << std::endl;
        dlclose(hLib);
        hLib = nullptr;
    }
    curl_global_cleanup();
#endif
    
    std::cout << "[OK] All resources cleaned up" << std::endl;
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "Test completed!" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
