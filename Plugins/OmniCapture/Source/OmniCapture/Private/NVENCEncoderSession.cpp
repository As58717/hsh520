// Copyright Epic Games, Inc. All Rights Reserved.

#include "NVENCEncoderSession.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Logging/LogMacros.h"
#include "Windows/WindowsHWrapper.h"

// 定义日志类别
DECLARE_LOG_CATEGORY_EXTERN(LogOmniCaptureNVENC, Log, All);
DEFINE_LOG_CATEGORY(LogOmniCaptureNVENC);

// NVENC API定义（简化版本，实际使用时需要完整的nvEncodeAPI.h头文件）
#define NVENCAPI_FUNCTION_LIST_VER 12

struct NV_ENCODE_API_FUNCTION_LIST
{
    uint32_t version;
    // 简化的函数指针定义，实际实现需要完整的函数列表
    void* NvEncOpenEncodeSessionEx;
    void* NvEncCloseEncodeSession;
    void* NvEncCreateEncoder;
    void* NvEncDestroyEncoder;
    void* NvEncRegisterResource;
    void* NvEncUnregisterResource;
    void* NvEncEncodePicture;
    void* NvEncLockBitstream;
    void* NvEncUnlockBitstream;
    void* NvEncCreateInputBuffer;
    void* NvEncDestroyInputBuffer;
    void* NvEncGetEncodeCaps;
    void* NvEncGetEncodePresetConfig;
    void* NvEncSetEncodeConfig;
    void* NvEncInitializeEncoder;
    void* NvEncGetSequenceParams;
    void* NvEncGetPictureParams;
    void* NvEncFlushEncoder;
};

// 静态成员初始化
void* FNVENCEncoderSession::NVENCModuleHandle = nullptr;

FNVENCEncoderSession::FNVENCEncoderSession()
{
}

FNVENCEncoderSession::~FNVENCEncoderSession()
{
    Shutdown();
}

bool FNVENCEncoderSession::Initialize(const FIntPoint& InResolution, EOmniCaptureCodec InCodec, const FOmniCaptureQuality& InQuality, EOmniCaptureColorFormat InColorFormat)
{
    UE_LOG(LogOmniCaptureNVENC, Log, TEXT("Initializing NVENC encoder session with resolution %dx%d, codec %d, color format %d"), 
           InResolution.X, InResolution.Y, (int32)InCodec, (int32)InColorFormat);

    Resolution = InResolution;
    Codec = InCodec;
    Quality = InQuality;
    ColorFormat = InColorFormat;

    // 加载NVENC API
    if (!LoadNVENCApi())
    {
        UE_LOG(LogOmniCaptureNVENC, Error, TEXT("Failed to load NVENC API"));
        return false;
    }

    // 创建NVENC设备
    if (!CreateNVENCDevice())
    {
        UE_LOG(LogOmniCaptureNVENC, Error, TEXT("Failed to create NVENC device"));
        return false;
    }

    // 创建编码器
    if (!CreateEncoder(InResolution, InCodec, InQuality, InColorFormat))
    {
        UE_LOG(LogOmniCaptureNVENC, Error, TEXT("Failed to create NVENC encoder"));
        return false;
    }

    // 分配编码资源
    if (!AllocateEncodingResources(InResolution, InColorFormat))
    {
        UE_LOG(LogOmniCaptureNVENC, Error, TEXT("Failed to allocate encoding resources"));
        return false;
    }

    bIsInitialized = true;
    UE_LOG(LogOmniCaptureNVENC, Log, TEXT("NVENC encoder session initialized successfully"));
    return true;
}

void FNVENCEncoderSession::Shutdown()
{
    if (!bIsInitialized)
    {
        return;
    }

    ReleaseEncodingResources();

    if (NvEncoder != nullptr && bIsEncoderCreated)
    {
        // 调用NvEncDestroyEncoder
        if (NvEncodeAPI && NvEncodeAPI->NvEncDestroyEncoder)
        {
            auto NvEncDestroyEncoder = (bool(*)(NV_ENC_ENCODER))NvEncodeAPI->NvEncDestroyEncoder;
            NvEncDestroyEncoder(NvEncoder);
        }
        NvEncoder = nullptr;
        bIsEncoderCreated = false;
    }

    if (NvEncDevice != nullptr && bIsDeviceCreated)
    {
        // 调用NvEncCloseEncodeSession
        if (NvEncodeAPI && NvEncodeAPI->NvEncCloseEncodeSession)
        {
            auto NvEncCloseEncodeSession = (bool(*)(NV_ENC_DEVICE))NvEncodeAPI->NvEncCloseEncodeSession;
            NvEncCloseEncodeSession(NvEncDevice);
        }
        NvEncDevice = nullptr;
        bIsDeviceCreated = false;
    }

    if (NvEncodeAPI != nullptr)
    {
        FMemory::Free(NvEncodeAPI);
        NvEncodeAPI = nullptr;
    }

    bIsInitialized = false;
    UE_LOG(LogOmniCaptureNVENC, Log, TEXT("NVENC encoder session shut down"));
}

bool FNVENCEncoderSession::LoadNVENCApi()
{
    // 尝试加载nvEncodeAPI64.dll
    FString NVENCDllPath = TEXT("nvEncodeAPI64.dll");
    
    // 首先尝试系统目录
    FString SystemPath = FPlatformMisc::GetWindowsSystemDir();
    FString FullNVENCDllPath = FPaths::Combine(SystemPath, NVENCDllPath);
    
    if (!FPaths::FileExists(FullNVENCDllPath))
    {
        // 如果系统目录中没有，尝试直接加载（依赖系统PATH）
        NVENCModuleHandle = FPlatformProcess::GetDllHandle(*NVENCDllPath);
    }
    else
    {
        NVENCModuleHandle = FPlatformProcess::GetDllHandle(*FullNVENCDllPath);
    }

    if (!NVENCModuleHandle)
    {
        UE_LOG(LogOmniCaptureNVENC, Error, TEXT("Failed to load %s"), *NVENCDllPath);
        return false;
    }

    // 获取NvEncodeAPIGetMaxSupportedVersion函数
    auto NvEncodeAPIGetMaxSupportedVersion = (uint32_t(*)(void))FPlatformProcess::GetDllExport(NVENCModuleHandle, TEXT("NvEncodeAPIGetMaxSupportedVersion"));
    if (!NvEncodeAPIGetMaxSupportedVersion)
    {
        UE_LOG(LogOmniCaptureNVENC, Error, TEXT("Failed to get NvEncodeAPIGetMaxSupportedVersion"));
        return false;
    }

    // 获取最大支持的API版本
    uint32_t MaxAPIVersion = NvEncodeAPIGetMaxSupportedVersion();
    UE_LOG(LogOmniCaptureNVENC, Log, TEXT("NVENC maximum supported API version: %d"), MaxAPIVersion);

    // 创建API函数列表
    NvEncodeAPI = (PNV_ENCODE_API_FUNCTION_LIST)FMemory::Malloc(sizeof(NV_ENCODE_API_FUNCTION_LIST));
    NvEncodeAPI->version = NVENCAPI_FUNCTION_LIST_VER;

    // 获取NvEncodeAPICreateInstance函数
    auto NvEncodeAPICreateInstance = (bool(*)(PNV_ENCODE_API_FUNCTION_LIST))FPlatformProcess::GetDllExport(NVENCModuleHandle, TEXT("NvEncodeAPICreateInstance"));
    if (!NvEncodeAPICreateInstance)
    {
        UE_LOG(LogOmniCaptureNVENC, Error, TEXT("Failed to get NvEncodeAPICreateInstance"));
        return false;
    }

    // 初始化API函数列表
    if (!NvEncodeAPICreateInstance(NvEncodeAPI))
    {
        UE_LOG(LogOmniCaptureNVENC, Error, TEXT("Failed to initialize NVENC API function list"));
        return false;
    }

    UE_LOG(LogOmniCaptureNVENC, Log, TEXT("NVENC API loaded successfully, version: %d"), NvEncodeAPI->version);
    return true;
}

bool FNVENCEncoderSession::CreateNVENCDevice()
{
    // 简化实现，实际需要从D3D设备创建NVENC设备
    // 这里只是示例，真实实现需要获取UE的D3D11或D3D12设备
    UE_LOG(LogOmniCaptureNVENC, Warning, TEXT("CreateNVENCDevice: This is a simplified implementation. Need to get D3D device from UE"));
    
    // 注意：实际实现中，需要从UE的RHI系统获取D3D设备
    // 例如对于D3D11：
    // ID3D11Device* D3DDevice = (ID3D11Device*)GDynamicRHI->RHIGetNativeDevice();
    
    // 暂时返回成功，实际实现需要正确创建NVENC设备
    bIsDeviceCreated = true;
    return true;
}

bool FNVENCEncoderSession::CreateEncoder(const FIntPoint& InResolution, EOmniCaptureCodec InCodec, const FOmniCaptureQuality& InQuality, EOmniCaptureColorFormat InColorFormat)
{
    // 简化实现，实际需要设置详细的编码器参数
    UE_LOG(LogOmniCaptureNVENC, Warning, TEXT("CreateEncoder: This is a simplified implementation"));
    
    // 实际实现中，需要调用NvEncCreateEncoder和相关函数设置编码器参数
    // 包括编码器类型、分辨率、码率控制等
    
    bIsEncoderCreated = true;
    return true;
}

bool FNVENCEncoderSession::AllocateEncodingResources(const FIntPoint& InResolution, EOmniCaptureColorFormat InColorFormat)
{
    // 简化实现，实际需要分配NVENC输入资源
    UE_LOG(LogOmniCaptureNVENC, Warning, TEXT("AllocateEncodingResources: This is a simplified implementation"));
    
    // 实际实现中，需要调用NvEncRegisterResource等函数注册输入资源
    
    return true;
}

void FNVENCEncoderSession::ReleaseEncodingResources()
{
    // 释放输入资源
    for (auto& Resource : InputResources)
    {
        if (Resource != nullptr && NvEncodeAPI && NvEncodeAPI->NvEncUnregisterResource)
        {
            auto NvEncUnregisterResource = (bool(*)(NV_ENC_ENCODER, NV_ENC_INPUT_RESOURCE))NvEncodeAPI->NvEncUnregisterResource;
            NvEncUnregisterResource(NvEncoder, Resource);
        }
    }
    InputResources.Empty();

    // 释放设备缓冲区
    for (auto& Buffer : DeviceBuffers)
    {
        if (Buffer != nullptr)
        {
            // 释放缓冲区
            FMemory::Free(Buffer);
        }
    }
    DeviceBuffers.Empty();

    // 释放暂存纹理
    StagingTextures.Empty();
}

bool FNVENCEncoderSession::EncodeTexture(const FTextureRHIRef& InTexture, double InTimestamp, bool bIsKeyFrame, TArray<uint8>& OutBitstream)
{
    // 简化实现，实际需要将UE纹理转换为NVENC输入并编码
    UE_LOG(LogOmniCaptureNVENC, Warning, TEXT("EncodeTexture: This is a simplified implementation"));
    
    // 实际实现中，需要：
    // 1. 将UE纹理转换为NVENC输入格式
    // 2. 设置编码参数
    // 3. 执行编码
    // 4. 锁定并获取比特流
    
    return true;
}

bool FNVENCEncoderSession::EncodeBuffer(const void* InBuffer, const FIntPoint& InResolution, EPixelFormat InFormat, double InTimestamp, bool bIsKeyFrame, TArray<uint8>& OutBitstream)
{
    // 简化实现，实际需要将CPU缓冲区转换为NVENC输入并编码
    UE_LOG(LogOmniCaptureNVENC, Warning, TEXT("EncodeBuffer: This is a simplified implementation"));
    
    return true;
}

bool FNVENCEncoderSession::Flush(TArray<uint8>& OutBitstream)
{
    // 简化实现，实际需要刷新编码器队列
    UE_LOG(LogOmniCaptureNVENC, Warning, TEXT("Flush: This is a simplified implementation"));
    
    return true;
}

bool FNVENCEncoderSession::IsConfigurationSupported(EOmniCaptureCodec InCodec, EOmniCaptureColorFormat InColorFormat, const FIntPoint& InResolution)
{
    // 简化实现，实际需要查询NVENC能力
    UE_LOG(LogOmniCaptureNVENC, Warning, TEXT("IsConfigurationSupported: This is a simplified implementation"));
    
    // 基本检查
    if (InResolution.X <= 0 || InResolution.Y <= 0 || InResolution.X > 8192 || InResolution.Y > 8192)
    {
        return false;
    }
    
    return true;
}

FNVENCEncoderSession::FCapabilities FNVENCEncoderSession::GetCapabilities()
{
    FCapabilities Capabilities;
    
    // 简化实现，实际需要查询NVENC能力
    Capabilities.bSupportsH264 = true;
    Capabilities.bSupportsHEVC = true;
    Capabilities.bSupportsNV12 = true;
    Capabilities.bSupportsP010 = true;
    Capabilities.MaxWidth = 8192;
    Capabilities.MaxHeight = 8192;
    Capabilities.MaxBitrateKbps = 1000000;
    
    return Capabilities;
}

// FNVENCFrameContext实现
FNVENCFrameContext::FNVENCFrameContext()
{
}

FNVENCFrameContext::~FNVENCFrameContext()
{
}