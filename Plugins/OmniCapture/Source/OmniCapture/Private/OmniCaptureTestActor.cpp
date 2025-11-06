// Copyright Epic Games, Inc. All Rights Reserved.

#include "OmniCaptureTestActor.h"
#include "OmniCaptureRenderer.h"
#include "OmniCaptureEncoderFactory.h"
#include "OmniCaptureNVENCEncoderDirect.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Logging/LogMacros.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogOmniCaptureTest, Log, All);

AOmniCaptureTestActor::AOmniCaptureTestActor()
    : CaptureComponent(nullptr)
    , CaptureStartTime(0.0f)
    , LastStatsUpdateTime(0.0f)
    , AverageBitrate(0.0f)
    , CurrentFPS(0.0f)
    , EncodedFramesCount(0)
    , TotalEncodedDataSize(0.0)
    , CaptureResolution(1920, 1080)
    , CaptureFrameRate(60.0f)
    , CaptureDuration(0.0f)
    , bEnableHDR(false)
    , bAutoStartCapture(false)
    , bShowEncodingStats(true)
    , StatsUpdateFrequency(1.0f)
{
    // 设置Actor的属性
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    
    // 设置默认输出路径
    OutputFilePath = FPaths::ProjectSavedDir() + TEXT("Recordings/capture_") + FDateTime::Now().ToString() + TEXT(".mp4");
    
    // 初始化默认配置
    InitializeDefaultConfig();
}

void AOmniCaptureTestActor::BeginPlay()
{
    Super::BeginPlay();
    
    // 创建并初始化捕获组件
    CaptureComponent = NewObject<UOmniCaptureRenderComponent>(this, TEXT("OmniCaptureComponent"));
    if (CaptureComponent)
    {
        CaptureComponent->RegisterComponent();
        
        // 配置捕获组件
        CaptureComponent->SetResolution(CaptureResolution);
        CaptureComponent->SetCaptureFrameRate(CaptureFrameRate);
        CaptureComponent->SetCaptureHDR(bEnableHDR);
        
        // 如果配置有效，应用到捕获组件
        if (CaptureConfig)
        {
            // 这里可以将UOmniCaptureNVENCConfig配置转换为编码器配置
            // 具体实现取决于配置系统设计
        }
        
        UE_LOG(LogOmniCaptureTest, Log, TEXT("Capture component initialized"));
    }
    else
    {
        UE_LOG(LogOmniCaptureTest, Error, TEXT("Failed to create capture component"));
    }
    
    // 如果设置了自动开始捕获，则开始
    if (bAutoStartCapture)
    {
        StartCaptureSequence();
    }
    
    // 初始化统计信息
    EncodedFramesCount = 0;
    TotalEncodedDataSize = 0.0;
    AverageBitrate = 0.0f;
    CurrentFPS = 0.0f;
    LastStatsUpdateTime = GetWorld()->GetTimeSeconds();
    
    UE_LOG(LogOmniCaptureTest, Log, TEXT("Test actor initialized. Output path: %s"), *OutputFilePath);
}

void AOmniCaptureTestActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // 更新编码统计信息
    if (bShowEncodingStats)
    {
        UpdateEncodingStatistics(DeltaTime);
    }
    
    // 检查捕获持续时间
    CheckCaptureDuration(DeltaTime);
}

void AOmniCaptureTestActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 停止捕获
    StopCaptureSequence();
    
    // 注销并清理捕获组件
    if (CaptureComponent)
    {
        CaptureComponent->UnregisterComponent();
        CaptureComponent = nullptr;
    }
    
    Super::EndPlay(EndPlayReason);
}

void AOmniCaptureTestActor::StartCaptureSequence()
{
    if (!CaptureComponent)
    {
        FString ErrorMsg = TEXT("Capture component not available");
        UE_LOG(LogOmniCaptureTest, Error, TEXT("%s"), *ErrorMsg);
        OnCaptureError(ErrorMsg);
        return;
    }
    
    // 确保输出目录存在
    FString OutputDirectory = FPaths::GetPath(OutputFilePath);
    if (!IPlatformFile::GetPlatformPhysical().DirectoryExists(*OutputDirectory))
    {
        if (!IPlatformFile::GetPlatformPhysical().CreateDirectoryTree(*OutputDirectory))
        {
            FString ErrorMsg = TEXT("Failed to create output directory: ") + OutputDirectory;
            UE_LOG(LogOmniCaptureTest, Error, TEXT("%s"), *ErrorMsg);
            OnCaptureError(ErrorMsg);
            return;
        }
    }
    
    // 开始捕获
    CaptureComponent->StartCapture();
    
    // 记录开始时间
    CaptureStartTime = GetWorld()->GetTimeSeconds();
    
    // 重置统计信息
    EncodedFramesCount = 0;
    TotalEncodedDataSize = 0.0;
    AverageBitrate = 0.0f;
    CurrentFPS = 0.0f;
    
    UE_LOG(LogOmniCaptureTest, Log, TEXT("Capture sequence started. Duration: %.1fs"), 
        CaptureDuration > 0.0f ? CaptureDuration : 0.0f);
}

void AOmniCaptureTestActor::StopCaptureSequence()
{
    if (!CaptureComponent)
    {
        UE_LOG(LogOmniCaptureTest, Warning, TEXT("Capture component not available to stop capture"));
        return;
    }
    
    if (CaptureComponent->IsCapturing())
    {
        CaptureComponent->StopCapture();
        
        // 调用完成回调
        OnCaptureCompleted(OutputFilePath);
        
        UE_LOG(LogOmniCaptureTest, Log, TEXT("Capture sequence stopped. Total encoded frames: %d"), EncodedFramesCount);
    }
}

void AOmniCaptureTestActor::PauseCaptureSequence()
{
    if (CaptureComponent)
    {
        CaptureComponent->PauseCapture();
        UE_LOG(LogOmniCaptureTest, Log, TEXT("Capture sequence paused"));
    }
}

void AOmniCaptureTestActor::ResumeCaptureSequence()
{
    if (CaptureComponent)
    {
        CaptureComponent->ResumeCapture();
        UE_LOG(LogOmniCaptureTest, Log, TEXT("Capture sequence resumed"));
    }
}

void AOmniCaptureTestActor::TestQualityPreset(EOmniCaptureQualityPreset Preset)
{
    if (!CaptureConfig)
    {
        InitializeDefaultConfig();
    }
    
    if (CaptureConfig)
    {
        // 应用质量预设
        CaptureConfig->ApplyQualityPreset(Preset);
        
        // 输出当前配置信息
        FString PresetName = TEXT("Unknown");
        switch (Preset)
        {
        case EOmniCaptureQualityPreset::Low:
            PresetName = TEXT("Low");
            break;
        case EOmniCaptureQualityPreset::Balanced:
            PresetName = TEXT("Balanced");
            break;
        case EOmniCaptureQualityPreset::High:
            PresetName = TEXT("High");
            break;
        case EOmniCaptureQualityPreset::Ultra:
            PresetName = TEXT("Ultra");
            break;
        case EOmniCaptureQualityPreset::Lossless:
            PresetName = TEXT("Lossless");
            break;
        default:
            break;
        }
        
        UE_LOG(LogOmniCaptureTest, Log, TEXT("Applied quality preset: %s"), *PresetName);
    }
}

void AOmniCaptureTestActor::TestResolution(FIntPoint Resolution)
{
    // 验证分辨率
    if (Resolution.X <= 0 || Resolution.Y <= 0)
    {
        FString ErrorMsg = TEXT("Invalid resolution");
        UE_LOG(LogOmniCaptureTest, Error, TEXT("%s"), *ErrorMsg);
        OnCaptureError(ErrorMsg);
        return;
    }
    
    // 更新分辨率
    CaptureResolution = Resolution;
    
    if (CaptureComponent)
    {
        CaptureComponent->SetResolution(Resolution);
    }
    
    UE_LOG(LogOmniCaptureTest, Log, TEXT("Test resolution set to %dx%d"), Resolution.X, Resolution.Y);
}

void AOmniCaptureTestActor::TestFrameRate(float FrameRate)
{
    // 验证帧率
    if (FrameRate <= 0.0f || FrameRate > 240.0f)
    {
        FString ErrorMsg = TEXT("Invalid frame rate (must be between 0.1 and 240)");
        UE_LOG(LogOmniCaptureTest, Error, TEXT("%s"), *ErrorMsg);
        OnCaptureError(ErrorMsg);
        return;
    }
    
    // 更新帧率
    CaptureFrameRate = FrameRate;
    
    if (CaptureComponent)
    {
        CaptureComponent->SetCaptureFrameRate(FrameRate);
    }
    
    UE_LOG(LogOmniCaptureTest, Log, TEXT("Test frame rate set to %.1f FPS"), FrameRate);
}

bool AOmniCaptureTestActor::IsCurrentlyCapturing() const
{
    return CaptureComponent && CaptureComponent->IsCapturing();
}

void AOmniCaptureTestActor::GetEncodingStatistics(float& OutAverageBitrate, float& OutCurrentFPS) const
{
    OutAverageBitrate = AverageBitrate;
    OutCurrentFPS = CurrentFPS;
}

void AOmniCaptureTestActor::UpdateEncodingStatistics(float DeltaTime)
{
    float CurrentTime = GetWorld()->GetTimeSeconds();
    
    // 检查是否需要更新统计信息
    if (CurrentTime - LastStatsUpdateTime >= StatsUpdateFrequency)
    {
        // 这里简化了统计信息计算
        // 实际项目中应该从编码器获取真实的统计数据
        
        // 模拟更新统计信息（在实际实现中，应该从编码器获取真实数据）
        if (EncodedFramesCount > 0 && TotalEncodedDataSize > 0.0)
        {
            // 计算平均比特率（Mbps）
            double Duration = CurrentTime - CaptureStartTime;
            if (Duration > 0.0)
            {
                AverageBitrate = static_cast<float>((TotalEncodedDataSize * 8.0) / (Duration * 1000000.0));
            }
            
            // 计算当前FPS
            if (StatsUpdateFrequency > 0.0)
            {
                CurrentFPS = EncodedFramesCount / StatsUpdateFrequency;
            }
            
            // 调用蓝图事件更新UI
            OnEncodingStatisticsUpdated(AverageBitrate, CurrentFPS);
            
            // 重置计数器
            EncodedFramesCount = 0;
            TotalEncodedDataSize = 0.0;
        }
        
        LastStatsUpdateTime = CurrentTime;
    }
}

void AOmniCaptureTestActor::CheckCaptureDuration(float DeltaTime)
{
    // 如果设置了捕获持续时间且正在捕获
    if (CaptureDuration > 0.0f && IsCurrentlyCapturing())
    {
        float CurrentTime = GetWorld()->GetTimeSeconds();
        
        // 检查是否达到了捕获持续时间
        if (CurrentTime - CaptureStartTime >= CaptureDuration)
        {
            StopCaptureSequence();
        }
    }
}

void AOmniCaptureTestActor::InitializeDefaultConfig()
{
    // 如果没有配置对象，创建一个新的
    if (!CaptureConfig)
    {
        CaptureConfig = NewObject<UOmniCaptureNVENCConfig>(this);
        if (CaptureConfig)
        {
            // 应用默认质量预设
            CaptureConfig->ApplyQualityPreset(EOmniCaptureQualityPreset::Balanced);
            
            UE_LOG(LogOmniCaptureTest, Log, TEXT("Default capture config created"));
        }
    }
}