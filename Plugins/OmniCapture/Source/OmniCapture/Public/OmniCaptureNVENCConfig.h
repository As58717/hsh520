// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OmniCaptureTypes.h"
#include "HAL/Platform.h"
#include "HAL/PlatformProcess.h"
#include "HAL/ThreadingBase.h"
#include "HAL/RunnableThread.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "OmniCaptureNVENCConfig.generated.h"

/**
 * NVENC编码器的详细配置选项
 */
UCLASS(BlueprintType, config=Engine, defaultconfig)
class OMNICAPTURE_API UOmniCaptureNVENCConfig : public UObject
{
    GENERATED_BODY()

public:
    UOmniCaptureNVENCConfig(const FObjectInitializer& ObjectInitializer);

    // 基本编码设置
    /** 视频编解码器 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Basic Encoding Settings")
    EOmniCaptureCodec Codec;

    /** 编码预设质量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Basic Encoding Settings", meta = (DisplayName = "Quality Preset"))
    EOmniCaptureQualityPreset QualityPreset;

    /** 目标比特率 (Kbps) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Basic Encoding Settings", meta = (DisplayName = "Target Bitrate (Kbps)", ClampMin = 1000, ClampMax = 100000))
    int32 TargetBitrateKbps;

    /** 最大比特率 (Kbps) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Basic Encoding Settings", meta = (DisplayName = "Max Bitrate (Kbps)", ClampMin = 1000, ClampMax = 100000))
    int32 MaxBitrateKbps;

    /** GOP大小 (关键帧间隔) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Basic Encoding Settings", meta = (DisplayName = "GOP Size", ClampMin = 1, ClampMax = 120))
    int32 GOPSize;

    /** B帧数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Basic Encoding Settings", meta = (DisplayName = "Number of B-Frames", ClampMin = 0, ClampMax = 5))
    int32 BFrameCount;

    /** 是否使用CBR编码模式 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Basic Encoding Settings", meta = (DisplayName = "Use CBR Mode"))
    bool bUseCBR;

    // 高级编码设置
    /** 是否启用CUDA加速 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Advanced Encoding Settings", meta = (DisplayName = "Enable CUDA Acceleration"))
    bool bEnableCUDA;

    /** 是否启用动态GOP */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Advanced Encoding Settings", meta = (DisplayName = "Enable Dynamic GOP"))
    bool bEnableDynamicGOP;

    /** 是否使用场景变化检测 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Advanced Encoding Settings", meta = (DisplayName = "Use Scene Change Detection"))
    bool bUseSceneChangeDetection;

    /** 最大编码延迟 (毫秒) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Advanced Encoding Settings", meta = (DisplayName = "Max Encoding Latency (ms)", ClampMin = 0, ClampMax = 1000))
    int32 MaxEncodingLatencyMs;

    /** 线程优先级 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Advanced Encoding Settings", meta = (DisplayName = "Thread Priority"))
    EThreadPriority EncodingThreadPriority;

    // 色彩设置
    /** 色彩格式 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Color Settings")
    EOmniCaptureColorFormat ColorFormat;

    /** 是否启用HDR支持 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Color Settings", meta = (DisplayName = "Enable HDR Support"))
    bool bEnableHDR;

    /** 色彩空间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Color Settings", meta = (DisplayName = "Color Space"))
    FString ColorSpace;

    /** 色彩范围 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Color Settings", meta = (DisplayName = "Color Range"))
    FString ColorRange;

    // 诊断和调试
    /** 是否启用诊断日志 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Diagnostics", meta = (DisplayName = "Enable Diagnostic Logging"))
    bool bEnableDiagnostics;

    /** 是否记录编码统计信息 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Diagnostics", meta = (DisplayName = "Log Encoding Statistics"))
    bool bLogEncodingStats;

    /** 每帧统计记录间隔 (帧率的分母) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = "Diagnostics", meta = (DisplayName = "Stats Log Interval", ClampMin = 1, ClampMax = 60))
    int32 StatsLogInterval;

public:
    /** 获取默认配置 */
    static UOmniCaptureNVENCConfig* GetDefault();

    /** 从配置生成FOmniCaptureQuality结构 */
    FOmniCaptureQuality GenerateCaptureQuality() const;

    /** 验证配置是否有效 */
    bool IsValidConfig() const;

    /** 根据编码器能力验证和调整配置 */
    void ValidateAndAdjustForCapabilities(const FOmniNVENCDirectCapabilities& Capabilities);

    /** 应用预设质量配置 */
    void ApplyQualityPreset(EOmniCaptureQualityPreset InPreset);
};