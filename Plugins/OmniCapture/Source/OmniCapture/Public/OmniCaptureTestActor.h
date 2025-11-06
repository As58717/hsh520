// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OmniCaptureNVENCConfig.h"
#include "OmniCaptureTestActor.generated.h"

UCLASS(Blueprintable, BlueprintType)
class OMNICAPTURE_API AOmniCaptureTestActor : public AActor
{
    GENERATED_BODY()
    
public:
    // 构造函数
    AOmniCaptureTestActor();
    
    // 从Actor继承的函数
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    
    // 开始捕获的函数
    UFUNCTION(BlueprintCallable, Category = "OmniCapture")
    void StartCaptureSequence();
    
    // 停止捕获的函数
    UFUNCTION(BlueprintCallable, Category = "OmniCapture")
    void StopCaptureSequence();
    
    // 暂停捕获的函数
    UFUNCTION(BlueprintCallable, Category = "OmniCapture")
    void PauseCaptureSequence();
    
    // 恢复捕获的函数
    UFUNCTION(BlueprintCallable, Category = "OmniCapture")
    void ResumeCaptureSequence();
    
    // 测试不同质量预设的函数
    UFUNCTION(BlueprintCallable, Category = "OmniCapture")
    void TestQualityPreset(EOmniCaptureQualityPreset Preset);
    
    // 测试不同分辨率的函数
    UFUNCTION(BlueprintCallable, Category = "OmniCapture")
    void TestResolution(FIntPoint Resolution);
    
    // 测试不同帧率的函数
    UFUNCTION(BlueprintCallable, Category = "OmniCapture")
    void TestFrameRate(float FrameRate);
    
    // 捕获完成后的回调
    UFUNCTION(BlueprintImplementableEvent, Category = "OmniCapture")
    void OnCaptureCompleted(const FString& OutputFilePath);
    
    // 捕获错误的回调
    UFUNCTION(BlueprintImplementableEvent, Category = "OmniCapture")
    void OnCaptureError(const FString& ErrorMessage);
    
    // 实时编码统计信息更新回调
    UFUNCTION(BlueprintImplementableEvent, Category = "OmniCapture")
    void OnEncodingStatisticsUpdated(float AverageBitrate, float CurrentFPS);
    
    // 是否正在捕获
    UFUNCTION(BlueprintCallable, Category = "OmniCapture")
    bool IsCurrentlyCapturing() const;
    
    // 获取当前编码统计信息
    UFUNCTION(BlueprintCallable, Category = "OmniCapture")
    void GetEncodingStatistics(float& OutAverageBitrate, float& OutCurrentFPS) const;
    
    // 公共属性
    
    // 捕获配置
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "OmniCapture")
    UOmniCaptureNVENCConfig* CaptureConfig;
    
    // 捕获分辨率
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "OmniCapture")
    FIntPoint CaptureResolution; 
    
    // 捕获帧率
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "OmniCapture")
    float CaptureFrameRate;
    
    // 捕获持续时间（秒），0表示不限时
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "OmniCapture")
    float CaptureDuration;
    
    // 输出文件路径
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "OmniCapture")
    FString OutputFilePath;
    
    // 是否启用HDR捕获
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "OmniCapture")
    bool bEnableHDR;
    
    // 是否在游戏开始时自动开始捕获
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "OmniCapture")
    bool bAutoStartCapture;
    
    // 是否显示编码统计信息
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "OmniCapture")
    bool bShowEncodingStats;
    
    // 统计信息更新频率（秒）
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "OmniCapture", meta = (ClampMin = "0.1", ClampMax = "5.0"))
    float StatsUpdateFrequency;
    
private:
    // 渲染组件
    UPROPERTY()
    UOmniCaptureRenderComponent* CaptureComponent;
    
    // 捕获开始时间
    float CaptureStartTime;
    
    // 上次统计信息更新时间
    float LastStatsUpdateTime;
    
    // 编码统计信息
    float AverageBitrate;
    float CurrentFPS;
    int32 EncodedFramesCount;
    double TotalEncodedDataSize;
    
    // 更新编码统计信息
    void UpdateEncodingStatistics(float DeltaTime);
    
    // 检查捕获持续时间
    void CheckCaptureDuration(float DeltaTime);
    
    // 初始化默认配置
    void InitializeDefaultConfig();
};