// Copyright Epic Games, Inc. All Rights Reserved.

#include "OmniCaptureNVENCAPI.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDeviceDebug.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformApplicationMisc.h"

// 定义NVENC API结构和函数
#define NVENCAPI_STATIC 1
#include <nvEncodeAPI.h>

// 静态常量定义
const FString FOmniCaptureNVENCAPI::NVEncodeAPIDLLName = TEXT("nvEncodeAPI64.dll");

const TArray<FString> FOmniCaptureNVENCAPI::SystemSearchPaths = {
    TEXT("C:\\Windows\\System32"),
    TEXT("C:\\Windows\\SysWOW64"),
    TEXT("C:\\Program Files\\NVIDIA Corporation\\NVSMI"),
    TEXT("C:\\Program Files (x86)\\NVIDIA Corporation\\NVSMI")
};

FOmniCaptureNVENCAPI::FOmniCaptureNVENCAPI()
    : NVEncodeAPILibraryHandle(nullptr)
    , NVEncodeAPIFuncList(nullptr)
    , bIsNVEncodeAPILoaded(false)
    , MaxEncoderSessions(0)
{
    // 初始化NVENC API函数列表
    NVEncodeAPIFuncList = MakeUnique<NV_ENCODE_API_FUNCTION_LIST>();
    NVEncodeAPIFuncList->version = NV_ENCODE_API_FUNCTION_LIST_VER;
}

FOmniCaptureNVENCAPI::~FOmniCaptureNVENCAPI()
{
    // 清理资源
    UnloadNVEncodeAPI();
}

FOmniCaptureNVENCAPI& FOmniCaptureNVENCAPI::Get()
{
    static FOmniCaptureNVENCAPI Singleton;
    return Singleton;
}

bool FOmniCaptureNVENCAPI::IsNVENCAvailable() const
{
    return bIsNVEncodeAPILoaded && CheckGPUHardwareSupport();
}

bool FOmniCaptureNVENCAPI::LoadNVEncodeAPI()
{
    // 如果已经加载，直接返回
    if (bIsNVEncodeAPILoaded)
    {
        return true;
    }

    // 首先尝试查找已设置的路径
    if (!NVEncodeAPILibraryPath.IsEmpty() && IsValidNVEncodeAPILibrary(NVEncodeAPILibraryPath))
    {
        // 使用已设置的路径
    }
    else
    {
        // 扫描系统查找NVENC动态库
        if (!ScanSystemForNVEncodeAPI())
        {
            UE_LOG(LogTemp, Warning, TEXT("NVENC API library not found in system paths"));
            return false;
        }
    }

    // 加载动态库
    NVEncodeAPILibraryHandle = LoadDynamicLibrary(NVEncodeAPILibraryPath);
    if (!NVEncodeAPILibraryHandle)
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to load NVENC API library: %s"), *NVEncodeAPILibraryPath);
        return false;
    }

    // 初始化API函数列表
    if (!InitializeNVEncodeAPIFunctions())
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to initialize NVENC API functions"));
        UnloadDynamicLibrary(NVEncodeAPILibraryHandle);
        NVEncodeAPILibraryHandle = nullptr;
        return false;
    }

    // 运行可用性测试
    if (!RunNVENCAvailabilityTest())
    {
        UE_LOG(LogTemp, Warning, TEXT("NVENC API availability test failed"));
        CleanupNVEncodeAPIFunctions();
        UnloadDynamicLibrary(NVEncodeAPILibraryHandle);
        NVEncodeAPILibraryHandle = nullptr;
        return false;
    }

    // 设置已加载标志
    bIsNVEncodeAPILoaded = true;
    UE_LOG(LogTemp, Log, TEXT("Successfully loaded NVENC API from: %s"), *NVEncodeAPILibraryPath);
    UE_LOG(LogTemp, Log, TEXT("NVENC Driver Version: %s"), *GetNVENCDriverVersion());

    return true;
}

void FOmniCaptureNVENCAPI::UnloadNVEncodeAPI()
{
    if (bIsNVEncodeAPILoaded)
    {
        CleanupNVEncodeAPIFunctions();
        
        if (NVEncodeAPILibraryHandle)
        {
            UnloadDynamicLibrary(NVEncodeAPILibraryHandle);
            NVEncodeAPILibraryHandle = nullptr;
        }
        
        bIsNVEncodeAPILoaded = false;
        UE_LOG(LogTemp, Log, TEXT("NVENC API unloaded"));
    }
}

bool FOmniCaptureNVENCAPI::IsNVEncodeAPILoaded() const
{
    return bIsNVEncodeAPILoaded;
}

NV_ENCODE_API_FUNCTION_LIST* FOmniCaptureNVENCAPI::GetNVEncodeAPIFunctions()
{
    return NVEncodeAPIFuncList.Get();
}

FString FOmniCaptureNVENCAPI::GetNVENCDriverVersion() const
{
    return NVENCDriverVersion;
}

FString FOmniCaptureNVENCAPI::GetNVEncodeAPILibraryPath() const
{
    return NVEncodeAPILibraryPath;
}

bool FOmniCaptureNVENCAPI::ScanSystemForNVEncodeAPI()
{
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    
    // 首先检查系统路径
    for (const FString& SearchPath : SystemSearchPaths)
    {
        FString FullPath = FPaths::Combine(SearchPath, NVEncodeAPIDLLName);
        if (PlatformFile.FileExists(*FullPath))
        {
            if (IsValidNVEncodeAPILibrary(FullPath))
            {
                NVEncodeAPILibraryPath = FullPath;
                return true;
            }
        }
    }
    
    // 然后检查环境变量PATH中的路径
    FString PathEnv = FPlatformMisc::GetEnvironmentVariable(TEXT("PATH"));
    TArray<FString> PathEntries;
    PathEnv.ParseIntoArray(PathEntries, TEXT(";"), true);
    
    for (const FString& PathEntry : PathEntries)
    {
        FString FullPath = FPaths::Combine(PathEntry, NVEncodeAPIDLLName);
        if (PlatformFile.FileExists(*FullPath))
        {
            if (IsValidNVEncodeAPILibrary(FullPath))
            {
                NVEncodeAPILibraryPath = FullPath;
                return true;
            }
        }
    }
    
    return false;
}

bool FOmniCaptureNVENCAPI::IsValidNVEncodeAPILibrary(const FString& LibraryPath) const
{
    // 尝试加载库来验证
    void* TempHandle = LoadDynamicLibrary(LibraryPath);
    if (TempHandle)
    {
        // 检查关键函数是否存在
        bool bHasGetProcAddress = (GetProcAddress(TempHandle, "NvEncodeAPIGetProcAddress") != nullptr);
        UnloadDynamicLibrary(TempHandle);
        return bHasGetProcAddress;
    }
    return false;
}

bool FOmniCaptureNVENCAPI::SetNVEncodeAPILibraryPath(const FString& LibraryPath)
{
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    
    if (PlatformFile.FileExists(*LibraryPath))
    {
        if (IsValidNVEncodeAPILibrary(LibraryPath))
        {
            // 如果已经加载，先卸载
            if (bIsNVEncodeAPILoaded)
            {
                UnloadNVEncodeAPI();
            }
            
            NVEncodeAPILibraryPath = LibraryPath;
            return true;
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Invalid NVENC API library path: %s"), *LibraryPath);
    return false;
}

int32 FOmniCaptureNVENCAPI::GetMaxEncoderSessions() const
{
    return MaxEncoderSessions;
}

FString FOmniCaptureNVENCAPI::GetNVENCHardwareInfo() const
{
    return NVENCHardwareInfo;
}

bool FOmniCaptureNVENCAPI::RunNVENCAvailabilityTest()
{
    // 创建编码器实例进行测试
    void* NvEncoder = nullptr;
    NV_ENCODE_API_FUNCTION_LIST* NvEncodeAPI = NVEncodeAPIFuncList.Get();
    
    // 初始化编码器
    NVENCSTATUS Status = NvEncodeAPI->NvEncInitializeEncoder(nullptr, &NvEncoder, nullptr);
    if (Status != NV_ENC_SUCCESS)
    {
        // 获取硬件信息
        uint32_t EncoderGUIDCount = 0;
        NvEncodeAPI->NvEncGetEncodeGUIDCount(nullptr, &EncoderGUIDCount);
        
        // 获取最大会话数
        uint32_t MaxSessions = 0;
        NvEncodeAPI->NvEncGetMaxEncodeSessionCount(nullptr, &MaxSessions);
        MaxEncoderSessions = MaxSessions;
        
        // 构建硬件信息字符串
        NVENCHardwareInfo = FString::Printf(TEXT("GPU Supports Encoding, Max Sessions: %d"), MaxSessions);
        
        // 获取驱动版本
        uint32_t DriverVersion = 0;
        NvEncodeAPI->NvEncGetDriverVersion(nullptr, &DriverVersion);
        NVENCDriverVersion = FString::Printf(TEXT("%d"), DriverVersion);
        
        return true;
    }
    
    // 如果成功初始化，清理
    if (NvEncoder)
    {
        NvEncodeAPI->NvEncDestroyEncoder(NvEncoder);
    }
    
    return true;
}

void* FOmniCaptureNVENCAPI::LoadDynamicLibrary(const FString& LibraryPath) const
{
    return FPlatformProcess::GetDllHandle(*LibraryPath);
}

void FOmniCaptureNVENCAPI::UnloadDynamicLibrary(void* LibraryHandle) const
{
    if (LibraryHandle)
    {
        FPlatformProcess::FreeDllHandle(LibraryHandle);
    }
}

void* FOmniCaptureNVENCAPI::GetProcAddress(void* LibraryHandle, const char* ProcName) const
{
    return FPlatformProcess::GetDllExport(LibraryHandle, ProcName);
}

bool FOmniCaptureNVENCAPI::InitializeNVEncodeAPIFunctions()
{
    // 首先获取NvEncodeAPIGetProcAddress函数
    void* GetProcAddressFunc = GetProcAddress(NVEncodeAPILibraryHandle, "NvEncodeAPIGetProcAddress");
    if (!GetProcAddressFunc)
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to get NvEncodeAPIGetProcAddress"));
        return false;
    }
    
    // 定义函数指针类型
    typedef NVENCSTATUS(NVENCAPI_CALL* PFNNVENCODEGETPROCADDRESS)(void*, const char*, void**);
    PFNNVENCODEGETPROCADDRESS NvEncodeAPIGetProcAddress = (PFNNVENCODEGETPROCADDRESS)GetProcAddressFunc;
    
    // 初始化函数列表
    NV_ENCODE_API_FUNCTION_LIST* NvEncodeAPI = NVEncodeAPIFuncList.Get();
    
    // 获取所有需要的函数
    #define GET_ENCODE_API_FUNCTION(FunctionName) \
    if (NvEncodeAPIGetProcAddress(nullptr, #FunctionName, (void**)&NvEncodeAPI->FunctionName) != NV_ENC_SUCCESS) \
    { \
        UE_LOG(LogTemp, Warning, TEXT("Failed to get NVENC API function: %s"), TEXT(#FunctionName)); \
        return false; \
    }
    
    // 获取核心NVENC API函数
    GET_ENCODE_API_FUNCTION(NvEncInitializeEncoder);
    GET_ENCODE_API_FUNCTION(NvEncDestroyEncoder);
    GET_ENCODE_API_FUNCTION(NvEncGetEncodeGUIDCount);
    GET_ENCODE_API_FUNCTION(NvEncGetEncodeGUID);
    GET_ENCODE_API_FUNCTION(NvEncGetMaxEncodeSessionCount);
    GET_ENCODE_API_FUNCTION(NvEncGetDriverVersion);
    GET_ENCODE_API_FUNCTION(NvEncCreateInputBuffer);
    GET_ENCODE_API_FUNCTION(NvEncDestroyInputBuffer);
    GET_ENCODE_API_FUNCTION(NvEncCreateBitstreamBuffer);
    GET_ENCODE_API_FUNCTION(NvEncDestroyBitstreamBuffer);
    GET_ENCODE_API_FUNCTION(NvEncEncodePicture);
    GET_ENCODE_API_FUNCTION(NvEncLockBitstream);
    GET_ENCODE_API_FUNCTION(NvEncUnlockBitstream);
    GET_ENCODE_API_FUNCTION(NvEncGetEncodePresetCount);
    GET_ENCODE_API_FUNCTION(NvEncGetEncodePresetGUID);
    GET_ENCODE_API_FUNCTION(NvEncGetEncodeProfileCount);
    GET_ENCODE_API_FUNCTION(NvEncGetEncodeProfileGUID);
    GET_ENCODE_API_FUNCTION(NvEncGetEncodeLevelCount);
    GET_ENCODE_API_FUNCTION(NvEncGetEncodeLevelGUID);
    GET_ENCODE_API_FUNCTION(NvEncGetEncodeCapability);
    
    #undef GET_ENCODE_API_FUNCTION
    
    return true;
}

void FOmniCaptureNVENCAPI::CleanupNVEncodeAPIFunctions()
{
    if (NVEncodeAPIFuncList)
    {
        // 重置所有函数指针
        FMemory::Memzero(NVEncodeAPIFuncList.Get(), sizeof(NV_ENCODE_API_FUNCTION_LIST));
        NVEncodeAPIFuncList->version = NV_ENCODE_API_FUNCTION_LIST_VER;
    }
}

bool FOmniCaptureNVENCAPI::CheckGPUHardwareSupport() const
{
    // 检查最大会话数是否大于0
    return MaxEncoderSessions > 0;
}

// 模块实现
void FOmniCaptureNVENCAPIModule::StartupModule()
{
    // 模块启动时尝试加载NVENC API
    UE_LOG(LogTemp, Log, TEXT("OmniCapture NVENC API Module Started"));
    
    // 预加载NVENC API（可选）
    FOmniCaptureNVENCAPI::Get().LoadNVEncodeAPI();
}

void FOmniCaptureNVENCAPIModule::ShutdownModule()
{
    // 模块关闭时卸载NVENC API
    UE_LOG(LogTemp, Log, TEXT("OmniCapture NVENC API Module Shutdown"));
    FOmniCaptureNVENCAPI::Get().UnloadNVEncodeAPI();
}

IMPLEMENT_MODULE(FOmniCaptureNVENCAPIModule, OmniCaptureNVENCAPIModule)
