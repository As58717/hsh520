// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RHIResources.h"
#include "RenderGraphBuilder.h"
#include "RendererInterface.h"
#include "Containers/Queue.h"
#include "OmniCaptureTypes.h"
#include "Components/ActorComponent.h"
#include "OmniCaptureRenderer.generated.h"

class IOmniCaptureEncoder;

/**
 * 渲染捕获配置结构
 */
USTRUCT(BlueprintType)
struct OMNICAPTURE_API FOmniCaptureRendererConfig
{
    GENERATED_BODY()
    
    // 捕获分辨率
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OmniCapture")
    FIntPoint Resolution;
    
    // 是否捕获HDR内容
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OmniCapture")
    bool bCaptureHDR;
    
    // 是否捕获Alpha通道
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OmniCapture")
    bool bCaptureAlpha;
    
    // 捕获频率（帧率）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OmniCapture")
    float CaptureFrequency;
    
    // 是否限制帧率
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OmniCapture")
    bool bLimitFrameRate;
    
    // 是否使用抗锯齿
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OmniCapture")
    bool bEnableAntiAliasing;
    
    // 是否使用自定义渲染设置
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OmniCapture")
    bool bUseCustomRenderSettings;
    
    // 自定义渲染设置（可以包含后期处理等）
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OmniCapture")
    FString CustomRenderSettings;

    // 默认构造函数
    FOmniCaptureRendererConfig()
        : Resolution(1920, 1080)
        , bCaptureHDR(false)
        , bCaptureAlpha(false)
        , CaptureFrequency(60.0f)
        , bLimitFrameRate(true)
        , bEnableAntiAliasing(true)
        , bUseCustomRenderSettings(false)
        , CustomRenderSettings(TEXT(""))
    {}
};

/**
 * 渲染捕获帧数据
 */
struct FOmniCaptureRenderFrame
{
    // 渲染目标引用
    TRefCountPtr<IPooledRenderTarget> RenderTarget;
    
    // GPU栅栏，用于同步
    FGPUFenceRHIRef Fence;
    
    // 时间戳
    double Timestamp;
    
    // 是否为关键帧
    bool bIsKeyFrame;
    
    // 帧索引
    int64 FrameIndex;
    
    // 默认构造函数
    FOmniCaptureRenderFrame()
        : Timestamp(0.0)
        , bIsKeyFrame(false)
        , FrameIndex(0)
    {}
};

/**
 * UE5渲染管线集成器，负责从渲染管线捕获帧并传递给编码器
 */
class OMNICAPTURE_API FOmniCaptureRenderer
{
public:
    /** 构造函数 */
    FOmniCaptureRenderer();
    
    /** 析构函数 */
    ~FOmniCaptureRenderer();

    /** 初始化渲染器 */
    bool Initialize(const FOmniCaptureRendererConfig& InConfig, TSharedPtr<IOmniCaptureEncoder> InEncoder);
    
    /** 关闭渲染器 */
    void Shutdown();
    
    /** 开始捕获 */
    void StartCapture();
    
    /** 停止捕获 */
    void StopCapture();
    
    /** 暂停捕获 */
    void PauseCapture();
    
    /** 恢复捕获 */
    void ResumeCapture();
    
    /** 更新配置 */
    void UpdateConfig(const FOmniCaptureRendererConfig& InConfig);
    
    /** 是否正在捕获 */
    bool IsCapturing() const;
    
    /** 是否暂停 */
    bool IsPaused() const;
    
    /** 获取当前配置 */
    const FOmniCaptureRendererConfig& GetConfig() const;
    
    /** 设置编码器 */
    void SetEncoder(TSharedPtr<IOmniCaptureEncoder> InEncoder);
    
    /** 获取编码器 */
    TSharedPtr<IOmniCaptureEncoder> GetEncoder() const;

    /** 在渲染线程中处理帧 */
    void ProcessFrame_RenderThread(FRHICommandListImmediate& RHICmdList, const FSceneView& View);

    /** 注册渲染事件处理程序 */
    void RegisterRenderEventHandlers();
    
    /** 注销渲染事件处理程序 */
    void UnregisterRenderEventHandlers();

private:
    /** 创建渲染目标 */
    void CreateRenderTargets();
    
    /** 释放渲染目标 */
    void ReleaseRenderTargets();
    
    /** 捕获帧到渲染目标 */
    void CaptureFrameToRenderTarget(FRHICommandListImmediate& RHICmdList, const FSceneView& View);
    
    /** 处理捕获的帧 */
    void ProcessCapturedFrame(const FOmniCaptureRenderFrame& Frame);
    
    /** 检查是否应该捕获当前帧（基于帧率限制） */
    bool ShouldCaptureCurrentFrame() const;
    
    /** 更新帧计时信息 */
    void UpdateFrameTiming();
    
    /** 创建GPU栅栏 */
    FGPUFenceRHIRef CreateGPUFence();
    
    /** 等待GPU栅栏 */
    void WaitForGPUFence(const FGPUFenceRHIRef& Fence);
    
    /** 转换渲染目标格式 */
    void ConvertRenderTargetFormat(FRHICommandListImmediate& RHICmdList, const TRefCountPtr<IPooledRenderTarget>& SourceRT, TRefCountPtr<IPooledRenderTarget>& DestRT);
    
    /** 处理编码后的帧 */
    void HandleEncodedFrame(const uint8* Data, uint32 Size, double Timestamp, bool bIsKeyFrame);

private:
    // 配置
    FOmniCaptureRendererConfig Config;
    
    // 编码器引用
    TSharedPtr<IOmniCaptureEncoder> Encoder;
    
    // 捕获状态
    bool bIsCapturing;
    bool bIsPaused;
    
    // 帧队列
    TQueue<FOmniCaptureRenderFrame, EQueueMode::Mpsc> CapturedFramesQueue;
    
    // 渲染目标
    TRefCountPtr<IPooledRenderTarget> CaptureRenderTarget;
    TRefCountPtr<IPooledRenderTarget> ConversionRenderTarget;
    
    // 帧计时信息
    double LastCaptureTime;
    double FrameTimeAccumulator;
    int64 TotalFramesRendered;
    int64 TotalFramesCaptured;
    
    // 渲染事件句柄
    FDelegateHandle OnEndFrameRenderHandle;
    
    // 互斥锁，用于保护共享数据
    mutable FCriticalSection CriticalSection;
    
    // 临时缓冲区，用于CPU帧数据
    TArray<uint8> TempFrameBuffer;
};

/**
 * 渲染捕获组件，在游戏中使用的蓝图可访问组件
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = (OmniCapture), meta = (BlueprintSpawnableComponent))
class OMNICAPTURE_API UOmniCaptureRenderComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UOmniCaptureRenderComponent(const FObjectInitializer& ObjectInitializer);
    
    // 从UActorComponent继承的方法
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnRegister() override;
    virtual void OnUnregister() override;

    // 蓝图可调用的方法
    /** 开始捕获 */
    UFUNCTION(BlueprintCallable, Category = "OmniCapture")
    void StartCapture();
    
    /** 停止捕获 */
    UFUNCTION(BlueprintCallable, Category = "OmniCapture")
    void StopCapture();
    
    /** 暂停捕获 */
    UFUNCTION(BlueprintCallable, Category = "OmniCapture")
    void PauseCapture();
    
    /** 恢复捕获 */
    UFUNCTION(BlueprintCallable, Category = "OmniCapture")
    void ResumeCapture();
    
    /** 设置捕获分辨率 */
    UFUNCTION(BlueprintCallable, Category = "OmniCapture")
    void SetResolution(FIntPoint NewResolution);
    
    /** 设置捕获帧率 */
    UFUNCTION(BlueprintCallable, Category = "OmniCapture")
    void SetCaptureFrameRate(float NewFrameRate);
    
    /** 设置是否捕获HDR */
    UFUNCTION(BlueprintCallable, Category = "OmniCapture")
    void SetCaptureHDR(bool bCaptureHDR);
    
    /** 设置输出格式 */
    UFUNCTION(BlueprintCallable, Category = "OmniCapture")
    void SetOutputFormat(EOmniOutputFormat NewFormat);
    
    /** 是否正在捕获 */
    UFUNCTION(BlueprintPure, Category = "OmniCapture")
    bool IsCapturing() const;
    
    /** 是否暂停 */
    UFUNCTION(BlueprintPure, Category = "OmniCapture")
    bool IsPaused() const;
    
    /** 获取当前配置 */
    UFUNCTION(BlueprintPure, Category = "OmniCapture")
    FOmniCaptureRendererConfig GetCurrentConfig() const;

    // 事件
    /** 开始捕获事件 */
    UFUNCTION(BlueprintImplementableEvent, Category = "OmniCapture", meta = (DisplayName = "OnCaptureStarted"))
    void ReceiveCaptureStarted();
    
    /** 停止捕获事件 */
    UFUNCTION(BlueprintImplementableEvent, Category = "OmniCapture", meta = (DisplayName = "OnCaptureStopped"))
    void ReceiveCaptureStopped();
    
    /** 捕获错误事件 */
    UFUNCTION(BlueprintImplementableEvent, Category = "OmniCapture", meta = (DisplayName = "OnCaptureError"))
    void ReceiveCaptureError(const FString& ErrorMessage);

protected:
    // 配置属性
    /** 捕获分辨率 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OmniCapture")
    FIntPoint CaptureResolution;
    
    /** 是否捕获HDR内容 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OmniCapture")
    bool bCaptureHDR;
    
    /** 是否捕获Alpha通道 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OmniCapture")
    bool bCaptureAlpha;
    
    /** 捕获帧率 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OmniCapture", meta = (ClampMin = 1.0, ClampMax = 120.0))
    float CaptureFrameRate;
    
    /** 是否限制帧率 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OmniCapture")
    bool bLimitFrameRate;
    
    /** 输出格式 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OmniCapture")
    EOmniOutputFormat OutputFormat;
    
    /** 是否自动开始捕获 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OmniCapture")
    bool bAutoStartCapture;

private:
    /** 渲染器实例 */
    TSharedPtr<FOmniCaptureRenderer> Renderer;
    
    /** 编码器实例 */
    TSharedPtr<IOmniCaptureEncoder> Encoder;
    
    /** 初始化渲染器和编码器 */
    void InitializeRenderer();
    
    /** 创建编码器 */
    TSharedPtr<IOmniCaptureEncoder> CreateEncoder();
    
    /** 处理错误 */
    void HandleError(const FString& ErrorMessage);
};