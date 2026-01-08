# WebGLHost Native SDK Demo

This directory contains a demo application that demonstrates how to integrate and use the WebGLHost Native SDK.

## Directory Structure

```
demo/
├── docs/                  # Documentation
│   ├── Quick-Start-Guide.md       # Quick start guide
│   └── API-Integration-Guide.md   # Detailed API reference
├── scripts/               # Build and run scripts
│   ├── build.bat              # Build demo.exe
│   ├── run.bat                # Run the test
│   ├── build-and-run.bat      # Build and run (all-in-one)
│   ├── clean.bat              # Clean build artifacts
│   ├── compress_split.ps1     # Split compression utility
│   ├── decompress_split.ps1   # Split decompression utility
│   └── USAGE.md               # Scripts usage guide
├── include/               # SDK header files
│   ├── IBrowsingService.h         # Browsing service interface
│   ├── IAppletManagerV3.h         # Applet manager interface
│   ├── ICoreServiceHandler.h      # Event handler interface
│   └── WIUnknown.h                # Base interface
├── native_sdk/            # Compressed SDK files (provided by vendor)
│   ├── host.zip               # SDK core DLL (compressed)
│   ├── runtime.z01            # Runtime files (split archive part 1)
│   ├── runtime.z02            # Runtime files (split archive part 2)
│   ├── runtime.z03            # Runtime files (split archive part 3)
│   └── runtime.z04            # Runtime files (split archive part 4)
├── host/                  # DLL directory (auto-extracted from native_sdk)
│   └── webglhost_export.dll       # SDK core DLL
├── runtime/               # Runtime environment (auto-extracted from native_sdk)
│   └── webglhost-runtime.exe      # Runtime executable
├── README.md              # This file
├── demo.cpp               # Demo source code
├── CMakeLists.txt         # CMake configuration
├── build/                 # Build output (generated)
│   └── bin/Release/demo.exe
└── logs/                  # Runtime logs (generated)
```

## Quick Start

### Option 1: All-in-One (Recommended)

```bat
scripts\build-and-run.bat
```

This will:
1. Build demo.exe
2. Run the test

### Option 2: Step-by-Step

```bat
# 1. Build demo.exe
scripts\build.bat

# 2. Run the test
scripts\run.bat
```

## Prerequisites

Before running the demo, ensure the SDK package is in place:

1. **SDK Package**: The `native_sdk/` directory contains compressed SDK files:
   - `host.zip` - The SDK core DLL (compressed)
   - `runtime.z01` ~ `runtime.z04` - Runtime environment (split compressed archive)

2. **Auto-Extraction**: The build and run scripts will automatically extract these files to:
   - `host/` directory - Contains `webglhost_export.dll`
   - `runtime/` directory - Contains `webglhost-runtime.exe` and other runtime files

**Note**: The compressed files are split into chunks of 50MB or less for easier distribution and version control. The extraction process is fully automated.

## Scripts Reference

All scripts are located in the `scripts/` directory.

### build.bat
Builds `demo.exe` using CMake and copies it to the demo directory.

**Usage**: `scripts\build.bat`

**Output**: 
- `build/bin/Release/demo.exe`
- `demo.exe` (copied to demo root)

### run.bat
Runs the demo application. Checks for all dependencies before running.

**Usage**: `scripts\run.bat`

**Requirements**:
- `demo.exe` must exist
- `host/webglhost_export.dll` must exist
- `runtime/webglhost-runtime.exe` must exist


### build-and-run.bat
All-in-one script that builds and runs the test.

**Usage**: `scripts\build-and-run.bat`

### clean.bat
Removes all build artifacts and optionally runtime and logs.

**Usage**: `scripts\clean.bat`

**Removes**:
- `build/` directory
- `demo.exe`
- `host/webglhost_export.dll`
- `runtime/` (optional)
- `logs/` (optional)

## Build Configuration

The demo uses CMake with Visual Studio 2019 (x64):

- **Compiler**: MSVC (Visual Studio 16 2019)
- **Platform**: x64
- **Configuration**: Release
- **C++ Standard**: C++17

## Test Configuration

The demo requires valid credentials. Edit `demo.cpp` and replace the placeholder values:

```cpp
const char* SDK_KEY = "your-sdk-key";        // Replace with your SDK key
const char* SDK_SECRET = "your-sdk-secret";  // Replace with your SDK secret
const char* ACCESS_TOKEN = "your-access-token"; // Replace with your access token
const char* USER_ID = "your-user-id";        // Replace with your user ID
const char* APP_ID = "your-app-id";          // Replace with your app ID
const char* LAUNCH_KEY = "https://...";      // Replace with your launch URL
```

Contact your SDK provider to obtain these credentials.

## Troubleshooting

### Error: DLL not found
The scripts will automatically extract `host/webglhost_export.dll` from `native_sdk/host.zip`. If the extraction fails, ensure `native_sdk/host.zip` exists and is not corrupted.

### Error: Runtime not found
The scripts will automatically extract the runtime from `native_sdk/runtime.z01~z04`. If the extraction fails, ensure all split archive files exist and are not corrupted.

### Error: Extraction failed
If automatic extraction fails:
1. Verify all files in `native_sdk/` directory are present and not corrupted
2. Check that PowerShell execution policy allows script execution
3. Manually extract files using the decompression script:
   ```bat
   powershell -ExecutionPolicy Bypass -File "scripts\decompress_split.ps1" -SourceDir "native_sdk" -OutputDir "." -ArchiveName "host"
   powershell -ExecutionPolicy Bypass -File "scripts\decompress_split.ps1" -SourceDir "native_sdk" -OutputDir "." -ArchiveName "runtime"
   ```

### Error: CMake not found
Install CMake: https://cmake.org/download/

### Error: Visual Studio not found
Install Visual Studio 2019 with C++ tools or modify `build.bat` to use your version:
```bat
cmake -G "Visual Studio 17 2022" -A x64 ..
```

## Logs

Runtime logs are automatically saved to the `logs/` directory with timestamps:
- Format: `server-YYYY-MM-DDTHH-MM-SS.log`
- Location: `demo/logs/`

Check the latest log file for runtime debugging information.

## Development

### Modifying demo.cpp

After making changes to `demo.cpp`, rebuild:

```bat
scripts\build.bat
```

### Changing configuration

Edit the constants at the top of `demo.cpp`:

```cpp
const char* SDK_KEY = "your-sdk-key";
const char* SDK_SECRET = "your-sdk-secret";
const char* ACCESS_TOKEN = "your-access-token";
const char* USER_ID = "your-user-id";
const char* APP_ID = "your-app-id";
const char* LAUNCH_KEY = "your-launch-key";
```

Then rebuild and run.

