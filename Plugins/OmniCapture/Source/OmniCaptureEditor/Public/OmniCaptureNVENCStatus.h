// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Framework/SlateDelegates.h"

class STextBlock;
class SProgressBar;
class FTimerHandle;

/**
 * NVENC 状态监控组件
 * 用于显示NVENC设备的可用性和状态信息
 */
class OM NICAPTUREEDITOR_API SOmniCaptureNVENCStatus : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SOmniCaptureNVENCStatus)
        : _bAutoRefresh(true)
        , _RefreshInterval(5.0f)
    {}
        
        /** 是否自动刷新状态 */
        SLATE_ARGUMENT(bool, bAutoRefresh)
        
        /** 刷新间隔（秒） */
        SLATE_ARGUMENT(float, RefreshInterval)
        
        /** 状态改变时的回调 */
        SLATE_EVENT(FOnNVENCStatusChanged, OnStatusChanged)
    SLATE_END_ARGS()

    /**
     * 构造函数
     */
    void Construct(const FArguments& InArgs);
    
    /**
     * 析构函数
     */
    virtual ~SOmniCaptureNVENCStatus();
    
    /**
     * 刷新NVENC状态
     */
    void RefreshNVENCStatus();
    
    /**
     * 检查NVENC是否可用
     */
    bool IsNVENCAvailable() const;
    
    /**
     * 获取NVENC驱动版本
     */
    FString GetDriverVersion() const;
    
    /**
     * 获取NVENC库路径
     */
    FString GetLibraryPath() const;
    
    /**
     * 获取最大编码会话数
     */
    int32 GetMaxEncoderSessions() const;
    
    /**
     * 获取硬件信息
     */
    FString GetHardwareInfo() const;
    
    /**
     * 手动扫描系统中的NVENC库
     */
    bool ScanForNVENCLibrary();
    
    /**
     * 手动设置NVENC库路径
     */
    bool SetNVENCLibraryPath(const FString& LibraryPath);
    
    /**
     * 启动自动刷新
     */
    void StartAutoRefresh(float Interval = 5.0f);
    
    /**
     * 停止自动刷新
     */
    void StopAutoRefresh();
    
    /**
     * 获取当前状态文本
     */
    FString GetStatusText() const;
    
    /**
     * 获取当前状态颜色
     */
    FSlateColor GetStatusColor() const;
    
    /**
     * 获取详细信息
     */
    FString GetDetailedInfo() const;

private:
    /**
     * 刷新定时器回调
     */
    void OnRefreshTimer();
    
    /**
     * 更新UI显示
     */
    void UpdateUIDisplay();
    
    /**
     * 检查NVENC API可用性
     */
    void CheckNVENCAvailability();
    
    /**
     * 获取状态图标
     */
    const FSlateBrush* GetStatusIcon() const;
    
    /**
     * 打开NVENC库路径选择对话框
     */
    void OnBrowseNVENCLibrary();
    
    /**
     * 重置到默认设置
     */
    void OnResetToDefault();
    
    /**
     * 测试NVENC功能
     */
    void OnTestNVENCFunctionality();
    
    /**
     * 状态文本显示
     */
    TSharedPtr<STextBlock> StatusTextBlock;
    
    /**
     * 详细信息文本显示
     */
    TSharedPtr<STextBlock> DetailedInfoTextBlock;
    
    /**
     * 驱动版本文本显示
     */
    TSharedPtr<STextBlock> DriverVersionTextBlock;
    
    /**
     * 库路径文本显示
     */
    TSharedPtr<STextBlock> LibraryPathTextBlock;
    
    /**
     * 会话数文本显示
     */
    TSharedPtr<STextBlock> SessionsTextBlock;
    
    /**
     * 刷新进度条
     */
    TSharedPtr<SProgressBar> RefreshProgressBar;
    
    /**
     * 刷新定时器句柄
     */
    TWeakPtr<FTimerHandle> RefreshTimerHandle;
    
    /**
     * 是否自动刷新
     */
    bool bAutoRefresh;
    
    /**
     * 刷新间隔
     */
    float RefreshInterval;
    
    /**
     * NVENC可用性状态
     */
    bool bNVENCAvailable;
    
    /**
     * NVENC驱动版本
     */
    FString DriverVersion;
    
    /**
     * NVENC库路径
     */
    FString LibraryPath;
    
    /**
     * 最大编码会话数
     */
    int32 MaxEncoderSessions;
    
    /**
     * 硬件信息
     */
    FString HardwareInfo;
    
    /**
     * 状态改变时的回调
     */
    FOnNVENCStatusChanged OnStatusChanged;
};

/**
 * NVENC状态改变委托
 */
DECLARE_DELEGATE_OneParam(FOnNVENCStatusChanged, bool /*bAvailable*/);

/**
 * NVENC设备信息
 */
struct FOmniCaptureNVENCDeviceInfo
{
    /** 是否可用 */
    bool bAvailable = false;
    
    /** 驱动版本 */
    FString DriverVersion;
    
    /** 库路径 */
    FString LibraryPath;
    
    /** 最大编码会话数 */
    int32 MaxEncoderSessions = 0;
    
    /** 硬件信息 */
    FString HardwareInfo;
    
    /** 支持的编码器列表 */
    TArray<FString> SupportedEncoders;
    
    /** 支持的分辨率列表 */
    TArray<FIntPoint> SupportedResolutions;
    
    /** 支持的最大帧率 */
    float MaxSupportedFrameRate = 0.0f;
};

/**
 * NVENC设备检测工具
 */
class OM NICAPTUREEDITOR_API FOmniCaptureNVENCDeviceDetector
{
public:
    /**
     * 获取设备信息
     */
    static FOmniCaptureNVENCDeviceInfo GetDeviceInfo();
    
    /**
     * 扫描系统中的NVENC设备
     */
    static bool ScanSystemDevices();
    
    /**
     * 运行NVENC功能测试
     */
    static bool RunFunctionalityTest();
    
    /**
     * 获取支持的编码器列表
     */
    static TArray<FString> GetSupportedEncoders();
    
    /**
     * 获取支持的分辨率列表
     */
    static TArray<FIntPoint> GetSupportedResolutions();
    
    /**
     * 检查特定分辨率是否支持
     */
    static bool IsResolutionSupported(const FIntPoint& Resolution);
    
    /**
     * 检查特定编码器是否支持
     */
    static bool IsEncoderSupported(const FString& EncoderName);
    
    /**
     * 获取最佳性能配置建议
     */
    static FString GetOptimalConfigurationRecommendation();
    
    /**
     * 获取诊断报告
     */
    static FString GenerateDiagnosticReport();
};
