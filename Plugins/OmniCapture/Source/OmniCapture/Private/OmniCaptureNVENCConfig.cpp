// Copyright Epic Games, Inc. All Rights Reserved.

#include "OmniCaptureNVENCConfig.h"
#include "Logging/LogMacros.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY_STATIC(LogOmniCaptureNVENCConfig, Log, All);

UOmniCaptureNVENCConfig::UOmniCaptureNVENCConfig(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
    , Codec(EOmniCaptureCodec::HEVC)
    , QualityPreset(EOmniCaptureQualityPreset::Balanced)
    , TargetBitrateKbps(10000)
    , MaxBitrateKbps(15000)
    , GOPSize(30)
    , BFrameCount(2)
    , bUseCBR(false)
    , bEnableCUDA(true)
    , bEnableDynamicGOP(true)
    , bUseSceneChangeDetection(true)
    , MaxEncodingLatencyMs(33)
    , EncodingThreadPriority(EOmniCaptureThreadPriority::Highest)
    , ColorFormat(EOmniCaptureColorFormat::P010)
    , bEnableHDR(false)
    , ColorSpace(TEXT("REC.709"))
    , ColorRange(TEXT("Limited"))
    , bEnableDiagnostics(false)
    , bLogEncodingStats(false)
    , StatsLogInterval(10)
{
    // 应用质量预设设置
    ApplyQualityPreset(QualityPreset);
}

UOmniCaptureNVENCConfig* UOmniCaptureNVENCConfig::GetDefault()
{
    return GetMutableDefault<UOmniCaptureNVENCConfig>();
}

FOmniCaptureQuality UOmniCaptureNVENCConfig::GenerateCaptureQuality() const
{
    FOmniCaptureQuality Quality;

    Quality.TargetBitrateKbps = TargetBitrateKbps;
    Quality.MaxBitrateKbps = MaxBitrateKbps;
    Quality.GOPLength = GOPSize;
    Quality.BFrames = BFrameCount;
    Quality.bLowLatency = bUseCBR;

    return Quality;
}

bool UOmniCaptureNVENCConfig::IsValidConfig() const
{
    // 基本验证
    if (TargetBitrateKbps <= 0 || MaxBitrateKbps <= 0)
    {
        UE_LOG(LogOmniCaptureNVENCConfig, Error, TEXT("Invalid bitrate settings: Target=%d, Max=%d"), TargetBitrateKbps, MaxBitrateKbps);
        return false;
    }
    
    if (TargetBitrateKbps > MaxBitrateKbps)
    {
        UE_LOG(LogOmniCaptureNVENCConfig, Warning, TEXT("Target bitrate exceeds max bitrate, adjusting..."));
    }
    
    if (GOPSize <= 0)
    {
        UE_LOG(LogOmniCaptureNVENCConfig, Error, TEXT("Invalid GOP size: %d"), GOPSize);
        return false;
    }
    
    if (BFrameCount < 0 || BFrameCount > 5)
    {
        UE_LOG(LogOmniCaptureNVENCConfig, Error, TEXT("Invalid B-frame count: %d"), BFrameCount);
        return false;
    }
    
    return true;
}

void UOmniCaptureNVENCConfig::ValidateAndAdjustForCapabilities(const FOmniNVENCDirectCapabilities& Capabilities)
{
    bool bAdjusted = false;
    
    // 验证编解码器支持
    if (Codec == EOmniCaptureCodec::HEVC && !Capabilities.bSupportsHEVC)
    {
        UE_LOG(LogOmniCaptureNVENCConfig, Warning, TEXT("HEVC not supported, falling back to H264"));
        Codec = EOmniCaptureCodec::H264;
        bAdjusted = true;
    }
    
    if (Codec == EOmniCaptureCodec::H264 && !Capabilities.bSupportsH264)
    {
        UE_LOG(LogOmniCaptureNVENCConfig, Error, TEXT("Neither HEVC nor H264 supported by NVENC"));
        Codec = EOmniCaptureCodec::H264; // 设置默认值，但可能会失败
    }
    
    // 验证色彩格式支持
    if (ColorFormat == EOmniCaptureColorFormat::P010 && !Capabilities.bSupportsP010)
    {
        UE_LOG(LogOmniCaptureNVENCConfig, Warning, TEXT("P010 color format not supported, falling back to NV12"));
        ColorFormat = EOmniCaptureColorFormat::NV12;
        bAdjusted = true;
    }
    
    if (ColorFormat == EOmniCaptureColorFormat::NV12 && !Capabilities.bSupportsNV12)
    {
        if (Capabilities.bSupportsBGRA)
        {
            UE_LOG(LogOmniCaptureNVENCConfig, Warning, TEXT("NV12 color format not supported, falling back to BGRA"));
            ColorFormat = EOmniCaptureColorFormat::BGRA;
            bAdjusted = true;
        }
        else
        {
            UE_LOG(LogOmniCaptureNVENCConfig, Error, TEXT("No supported color formats found"));
        }
    }
    
    // 如果启用了HDR但不支持，禁用HDR
    if (bEnableHDR && !Capabilities.bSupportsHDR)
    {
        UE_LOG(LogOmniCaptureNVENCConfig, Warning, TEXT("HDR not supported by NVENC, disabling"));
        bEnableHDR = false;
        bAdjusted = true;
    }

    // 验证B帧数量
    if (Capabilities.MaxBFrames > 0 && BFrameCount > Capabilities.MaxBFrames)
    {
        UE_LOG(LogOmniCaptureNVENCConfig, Warning, TEXT("B-frame count exceeds maximum supported (%d), adjusting to %d"),
            BFrameCount, Capabilities.MaxBFrames);
        BFrameCount = Capabilities.MaxBFrames;
        bAdjusted = true;
    }

    // 验证GOP大小
    if (Capabilities.MaxGOPSize > 0 && GOPSize > Capabilities.MaxGOPSize)
    {
        UE_LOG(LogOmniCaptureNVENCConfig, Warning, TEXT("GOP size exceeds maximum supported (%d), adjusting to %d"),
            GOPSize, Capabilities.MaxGOPSize);
        GOPSize = Capabilities.MaxGOPSize;
        bAdjusted = true;
    }
    
    if (bAdjusted)
    {
        UE_LOG(LogOmniCaptureNVENCConfig, Log, TEXT("Configuration adjusted to match NVENC capabilities"));
    }
}

void UOmniCaptureNVENCConfig::ApplyQualityPreset(EOmniCaptureQualityPreset InPreset)
{
    QualityPreset = InPreset;
    
    switch (InPreset)
    {
        case EOmniCaptureQualityPreset::Low:
            // 低质量，高速度
            bUseCBR = true;
            TargetBitrateKbps = 5000;
            MaxBitrateKbps = 7500;
            BFrameCount = 0;
            GOPSize = 60;
            bEnableDynamicGOP = false;
            bUseSceneChangeDetection = false;
            MaxEncodingLatencyMs = 17;
            break;
            
        case EOmniCaptureQualityPreset::Balanced:
            // 平衡质量和速度
            bUseCBR = false;
            TargetBitrateKbps = 10000;
            MaxBitrateKbps = 15000;
            BFrameCount = 2;
            GOPSize = 30;
            bEnableDynamicGOP = true;
            bUseSceneChangeDetection = true;
            MaxEncodingLatencyMs = 33;
            break;
            
        case EOmniCaptureQualityPreset::High:
            // 高质量，中等速度
            bUseCBR = false;
            TargetBitrateKbps = 15000;
            MaxBitrateKbps = 25000;
            BFrameCount = 3;
            GOPSize = 15;
            bEnableDynamicGOP = true;
            bUseSceneChangeDetection = true;
            MaxEncodingLatencyMs = 50;
            break;
            
        case EOmniCaptureQualityPreset::Ultra:
            // 极高质量，较低速度
            bUseCBR = false;
            TargetBitrateKbps = 25000;
            MaxBitrateKbps = 40000;
            BFrameCount = 4;
            GOPSize = 10;
            bEnableDynamicGOP = true;
            bUseSceneChangeDetection = true;
            MaxEncodingLatencyMs = 100;
            break;
            
        case EOmniCaptureQualityPreset::Lossless:
            // 无损质量，最低速度
            bUseCBR = false;
            TargetBitrateKbps = 50000;
            MaxBitrateKbps = 100000;
            BFrameCount = 0;
            GOPSize = 1;
            bEnableDynamicGOP = false;
            bUseSceneChangeDetection = true;
            MaxEncodingLatencyMs = 200;
            break;
            
        default:
            // 默认使用平衡预设
            ApplyQualityPreset(EOmniCaptureQualityPreset::Balanced);
            break;
    }
    
    UE_LOG(LogOmniCaptureNVENCConfig, Log, TEXT("Applied quality preset: %s"), *UEnum::GetValueAsString(InPreset));
}