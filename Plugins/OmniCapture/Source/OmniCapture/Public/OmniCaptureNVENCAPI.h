// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// NVENC API 定义
struct NV_ENCODE_API_FUNCTION_LIST;

/**
 * NVENC API 管理器类
 * 负责检测、加载和管理 NVENC API 动态链接库
 */
class OMNICAPTURE_API FOmniCaptureNVENCAPI
{
public:
    /** 构造函数 */
    FOmniCaptureNVENCAPI();
    
    /** 析构函数 */
    ~FOmniCaptureNVENCAPI();
    
    /** 静态单例获取函数 */
    static FOmniCaptureNVENCAPI& Get();
    
    /** 检测系统中是否安装了 NVENC */
    bool IsNVENCAvailable() const;
    
    /** 加载 NVENC API 动态库 */
    bool LoadNVEncodeAPI();
    
    /** 卸载 NVENC API 动态库 */
    void UnloadNVEncodeAPI();
    
    /** 检查是否已加载 NVENC API */
    bool IsNVEncodeAPILoaded() const;
    
    /** 获取 NVENC API 函数列表 */
    NV_ENCODE_API_FUNCTION_LIST* GetNVEncodeAPIFunctions();
    
    /** 获取 NVENC 驱动的版本信息 */
    FString GetNVENCDriverVersion() const;
    
    /** 获取 NVENC API 库路径 */
    FString GetNVEncodeAPILibraryPath() const;
    
    /** 扫描系统查找 NVENC 动态库 */
    bool ScanSystemForNVEncodeAPI();
    
    /** 检查指定路径是否存在有效的 NVENC 动态库 */
    bool IsValidNVEncodeAPILibrary(const FString& LibraryPath) const;
    
    /** 手动设置 NVENC 动态库路径 */
    bool SetNVEncodeAPILibraryPath(const FString& LibraryPath);
    
    /** 获取 NVENC 支持的最大编码会话数 */
    int32 GetMaxEncoderSessions() const;
    
    /** 获取 NVENC 硬件信息 */
    FString GetNVENCHardwareInfo() const;
    
    /** 运行 NVENC 可用性测试 */
    bool RunNVENCAvailabilityTest();
    
private:
    /** 加载动态库的通用函数 */
    void* LoadDynamicLibrary(const FString& LibraryPath) const;
    
    /** 卸载动态库 */
    void UnloadDynamicLibrary(void* LibraryHandle) const;
    
    /** 获取动态库中的函数指针 */
    void* GetProcAddress(void* LibraryHandle, const char* ProcName) const;
    
    /** 初始化 NVENC API 函数列表 */
    bool InitializeNVEncodeAPIFunctions();
    
    /** 清理 NVENC API 函数列表 */
    void CleanupNVEncodeAPIFunctions();
    
    /** 检查 GPU 硬件是否支持 NVENC */
    bool CheckGPUHardwareSupport() const;
    
    /** NVENC API 动态库句柄 */
    void* NVEncodeAPILibraryHandle;
    
    /** NVENC API 函数列表 */
    TUniquePtr<NV_ENCODE_API_FUNCTION_LIST> NVEncodeAPIFuncList;
    
    /** NVENC 动态库路径 */
    FString NVEncodeAPILibraryPath;
    
    /** 已加载标志 */
    bool bIsNVEncodeAPILoaded;
    
    /** 支持的最大编码会话数 */
    int32 MaxEncoderSessions;
    
    /** NVENC 驱动版本 */
    FString NVENCDriverVersion;
    
    /** GPU 硬件信息 */
    FString NVENCHardwareInfo;
    
    /** 默认的 NVENC 动态库名称 */
    static const FString NVEncodeAPIDLLName;
    
    /** 系统搜索路径列表 */
    static const TArray<FString> SystemSearchPaths;
};

/**
 * NVENC API 模块接口
 */
class FOmniCaptureNVENCAPIModule : public IModuleInterface
{
public:
    /** IModuleInterface 实现 */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
