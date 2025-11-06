// Copyright Epic Games, Inc. All Rights Reserved.

#include "OmniCaptureRenderer.h"
#include "OmniCaptureEncoderFactory.h"
#include "OmniCaptureNVENCEncoderDirect.h"
#include "OmniCaptureNVENCConfig.h"
#include "RenderGraphResources.h"
#include "Renderer/Private/ScenePrivate.h"
#include "Renderer/Private/PostProcess/PostProcessing.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogOmniCaptureRenderer, Log, All);

FOmniCaptureRenderer::FOmniCaptureRenderer()
    : Encoder(nullptr)
    , bIsCapturing(false)
    , bIsPaused(false)
    , LastCaptureTime(0.0)
    , FrameTimeAccumulator(0.0)
    , TotalFramesRendered(0)
    , TotalFramesCaptured(0)
{}

FOmniCaptureRenderer::~FOmniCaptureRenderer()
{
    Shutdown();
}

bool FOmniCaptureRenderer::Initialize(const FOmniCaptureRendererConfig& InConfig, TSharedPtr<IOmniCaptureEncoder> InEncoder)
{
    // 复制配置
    Config = InConfig;
    
    // 设置编码器
    Encoder = InEncoder;
    
    // 初始化状态
    bIsCapturing = false;
    bIsPaused = false;
    LastCaptureTime = 0.0;
    FrameTimeAccumulator = 0.0;
    TotalFramesRendered = 0;
    TotalFramesCaptured = 0;
    
    // 创建渲染目标
    CreateRenderTargets();
    
    // 注册渲染事件处理程序
    RegisterRenderEventHandlers();
    
    UE_LOG(LogOmniCaptureRenderer, Log, TEXT("Renderer initialized with resolution %dx%d"), Config.Resolution.X, Config.Resolution.Y);
    
    return true;
}

void FOmniCaptureRenderer::Shutdown()
{
    // 停止捕获
    StopCapture();
    
    // 注销渲染事件处理程序
    UnregisterRenderEventHandlers();
    
    // 释放渲染目标
    ReleaseRenderTargets();
    
    // 清空队列
    FOmniCaptureRenderFrame DummyFrame;
    while (CapturedFramesQueue.Dequeue(DummyFrame))
    {
        // 释放帧资源
        if (DummyFrame.Fence.IsValid())
        {
            WaitForGPUFence(DummyFrame.Fence);
        }
    }
    
    // 重置编码器引用
    Encoder = nullptr;
    
    UE_LOG(LogOmniCaptureRenderer, Log, TEXT("Renderer shut down"));
}

void FOmniCaptureRenderer::StartCapture()
{
    if (bIsCapturing)
    {
        UE_LOG(LogOmniCaptureRenderer, Warning, TEXT("Capture already in progress"));
        return;
    }
    
    if (!Encoder || !Encoder->IsInitialized())
    {
        UE_LOG(LogOmniCaptureRenderer, Error, TEXT("Cannot start capture: Encoder not initialized"));
        return;
    }
    
    // 重置计时和计数器
    LastCaptureTime = FPlatformTime::Seconds();
    FrameTimeAccumulator = 0.0;
    TotalFramesRendered = 0;
    TotalFramesCaptured = 0;
    
    // 开始捕获
    bIsCapturing = true;
    bIsPaused = false;
    
    UE_LOG(LogOmniCaptureRenderer, Log, TEXT("Capture started"));
}

void FOmniCaptureRenderer::StopCapture()
{
    if (!bIsCapturing)
    {
        UE_LOG(LogOmniCaptureRenderer, Warning, TEXT("No capture in progress"));
        return;
    }
    
    // 停止捕获
    bIsCapturing = false;
    bIsPaused = false;
    
    // 处理队列中的剩余帧
    ProcessCapturedFrameQueue();
    
    // 最终化编码器
    if (Encoder && Encoder->IsInitialized())
    {
        Encoder->Finalize([this](const uint8* Data, uint32 Size, double Timestamp, bool bIsKeyFrame) {
            HandleEncodedFrame(Data, Size, Timestamp, bIsKeyFrame);
        });
    }
    
    UE_LOG(LogOmniCaptureRenderer, Log, TEXT("Capture stopped. Rendered %lld frames, captured %lld frames"), 
        TotalFramesRendered, TotalFramesCaptured);
}

void FOmniCaptureRenderer::PauseCapture()
{
    if (!bIsCapturing)
    {
        UE_LOG(LogOmniCaptureRenderer, Warning, TEXT("No capture in progress to pause"));
        return;
    }
    
    bIsPaused = true;
    UE_LOG(LogOmniCaptureRenderer, Log, TEXT("Capture paused"));
}

void FOmniCaptureRenderer::ResumeCapture()
{
    if (!bIsCapturing || !bIsPaused)
    {
        UE_LOG(LogOmniCaptureRenderer, Warning, TEXT("Capture not paused"));
        return;
    }
    
    bIsPaused = false;
    LastCaptureTime = FPlatformTime::Seconds(); // 重置计时
    UE_LOG(LogOmniCaptureRenderer, Log, TEXT("Capture resumed"));
}

void FOmniCaptureRenderer::UpdateConfig(const FOmniCaptureRendererConfig& InConfig)
{
    FScopeLock Lock(&CriticalSection);
    
    // 更新配置
    bool bResolutionChanged = (Config.Resolution != InConfig.Resolution);
    Config = InConfig;
    
    // 如果分辨率改变，重新创建渲染目标
    if (bResolutionChanged && CaptureRenderTarget.IsValid())
    {
        ReleaseRenderTargets();
        CreateRenderTargets();
        
        UE_LOG(LogOmniCaptureRenderer, Log, TEXT("Resolution changed to %dx%d"), Config.Resolution.X, Config.Resolution.Y);
    }
}

bool FOmniCaptureRenderer::IsCapturing() const
{
    return bIsCapturing;
}

bool FOmniCaptureRenderer::IsPaused() const
{
    return bIsPaused;
}

const FOmniCaptureRendererConfig& FOmniCaptureRenderer::GetConfig() const
{
    return Config;
}

void FOmniCaptureRenderer::SetEncoder(TSharedPtr<IOmniCaptureEncoder> InEncoder)
{
    FScopeLock Lock(&CriticalSection);
    
    // 如果正在捕获，先停止
    if (bIsCapturing)
    {
        StopCapture();
    }
    
    // 设置新编码器
    Encoder = InEncoder;
}

TSharedPtr<IOmniCaptureEncoder> FOmniCaptureRenderer::GetEncoder() const
{
    return Encoder;
}

void FOmniCaptureRenderer::ProcessFrame_RenderThread(FRHICommandListImmediate& RHICmdList, const FSceneView& View)
{
    if (!bIsCapturing || bIsPaused)
    {
        return;
    }
    
    // 检查是否应该捕获当前帧
    if (!ShouldCaptureCurrentFrame())
    {
        TotalFramesRendered++;
        return;
    }
    
    // 更新帧计时
    UpdateFrameTiming();
    
    // 捕获帧到渲染目标
    CaptureFrameToRenderTarget(RHICmdList, View);
    
    TotalFramesRendered++;
}

void FOmniCaptureRenderer::RegisterRenderEventHandlers()
{
    // 注册场景颜色解析后的事件处理程序
    OnPostResolvedSceneColorHandle = FSceneRenderer::OnPostResolvedSceneColor.AddRaw(this, &FOmniCaptureRenderer::OnPostResolvedSceneColor);
    
    UE_LOG(LogOmniCaptureRenderer, Log, TEXT("Render event handlers registered"));
}

void FOmniCaptureRenderer::UnregisterRenderEventHandlers()
{
    // 注销事件处理程序
    if (OnPostResolvedSceneColorHandle.IsValid())
    {
        FSceneRenderer::OnPostResolvedSceneColor.Remove(OnPostResolvedSceneColorHandle);
        OnPostResolvedSceneColorHandle.Reset();
    }
    
    UE_LOG(LogOmniCaptureRenderer, Log, TEXT("Render event handlers unregistered"));
}

void FOmniCaptureRenderer::CreateRenderTargets()
{
    // 定义渲染目标参数
    FRHIResourceCreateInfo CreateInfo;
    CreateInfo.DebugName = TEXT("OmniCaptureRenderTarget");
    
    // 确定渲染目标格式
    EPixelFormat TargetFormat = Config.bCaptureHDR ? PF_A16B16G16R16F : PF_B8G8R8A8;
    
    // 创建主捕获渲染目标
    GRHICommandList.GetImmediateCommandList()->ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
    
    TPooledRenderTargetDesc CaptureRenderTargetDesc(FPooledRenderTargetDesc::Create2DDesc(
        Config.Resolution,
        TargetFormat,
        FClearValueBinding::None,
        TexCreate_RenderTargetable | TexCreate_ShaderResource,
        false
    ));
    
    CaptureRenderTargetDesc.DebugName = TEXT("OmniCapture.CaptureRenderTarget");
    
    if (!GRenderTargetPool.CreateUntrackedRenderTarget(RHICmdList, CaptureRenderTargetDesc, CaptureRenderTarget, TEXT("OmniCapture.CaptureRenderTarget")))
    {
        UE_LOG(LogOmniCaptureRenderer, Error, TEXT("Failed to create capture render target"));
        return;
    }
    
    // 创建转换渲染目标（用于格式转换）
    // 根据编码器支持的格式选择合适的目标格式
    EPixelFormat ConversionFormat = PF_B8G8R8A8; // 默认使用BGRA
    
    if (Encoder && Encoder->GetEncoderType() == EOmniOutputFormat::NVENCHardware)
    {
        // 获取NVENC能力
        auto Capabilities = FOmniCaptureNVENCEncoderDirect::GetNVENCCapabilities();
        
        // 根据能力选择最佳格式
        if (Capabilities.bSupportsP010 && Config.bCaptureHDR)
        {
            ConversionFormat = PF_P010;
        }
        else if (Capabilities.bSupportsNV12)
        {
            ConversionFormat = PF_NV12;
        }
    }
    
    if (TargetFormat != ConversionFormat)
    {
        TPooledRenderTargetDesc ConversionRenderTargetDesc(FPooledRenderTargetDesc::Create2DDesc(
            Config.Resolution,
            ConversionFormat,
            FClearValueBinding::None,
            TexCreate_RenderTargetable | TexCreate_ShaderResource,
            false
        ));
        
        ConversionRenderTargetDesc.DebugName = TEXT("OmniCapture.ConversionRenderTarget");
        
        if (!GRenderTargetPool.CreateUntrackedRenderTarget(RHICmdList, ConversionRenderTargetDesc, ConversionRenderTarget, TEXT("OmniCapture.ConversionRenderTarget")))
        {
            UE_LOG(LogOmniCaptureRenderer, Warning, TEXT("Failed to create conversion render target, will use main target"));
        }
    }
    
    UE_LOG(LogOmniCaptureRenderer, Log, TEXT("Render targets created: %dx%d, Format=%d"), 
        Config.Resolution.X, Config.Resolution.Y, TargetFormat);
}

void FOmniCaptureRenderer::ReleaseRenderTargets()
{
    if (CaptureRenderTarget.IsValid())
    {
        CaptureRenderTarget.SafeRelease();
    }
    
    if (ConversionRenderTarget.IsValid())
    {
        ConversionRenderTarget.SafeRelease();
    }
    
    UE_LOG(LogOmniCaptureRenderer, Log, TEXT("Render targets released"));
}

void FOmniCaptureRenderer::CaptureFrameToRenderTarget(FRHICommandListImmediate& RHICmdList, const FSceneView& View)
{
    if (!CaptureRenderTarget.IsValid())
    {
        UE_LOG(LogOmniCaptureRenderer, Error, TEXT("Capture render target not available"));
        return;
    }
    
    // 获取场景颜色纹理
    const FTextureRHIRef& SceneColorTexture = View.GetSceneColorTexture();
    if (!SceneColorTexture.IsValid())
    {
        UE_LOG(LogOmniCaptureRenderer, Error, TEXT("Scene color texture not available"));
        return;
    }
    
    // 调整大小并复制场景颜色到捕获渲染目标
    // 这里使用简单的复制，实际项目中可能需要更复杂的后处理
    const FTexture2DRHIRef& SrcTexture = SceneColorTexture->GetTexture2D();
    const FTexture2DRHIRef& DestTexture = CaptureRenderTarget->GetRenderTargetItem().TargetableTexture->GetTexture2D();
    
    // 创建临时采样器
    FSamplerStateRHIRef SamplerState = RHICreateSamplerState(FSamplerStateInitializerRHI(
        SF_Bilinear,
        AM_Clamp,
        AM_Clamp,
        AM_Clamp
    ));
    
    // 设置渲染目标
    RHICmdList.SetRenderTarget(0, CaptureRenderTarget->GetRenderTargetItem().TargetableTexture, FTextureRHIRef());
    
    // 清除渲染目标
    RHICmdList.Clear(true, FLinearColor::Black, false, 1.0f, false, 0, FIntRect());
    
    // 复制纹理
    RHICmdList.CopyTexture(
        SrcTexture, 0, 0,
        DestTexture, 0, 0,
        FIntPoint::ZeroValue,
        SrcTexture->GetSizeXYZ().XY()
    );
    
    // 创建GPU栅栏用于同步
    FGPUFenceRHIRef Fence = CreateGPUFence();
    RHICmdList.WriteGPUFence(Fence, 0);
    
    // 检查是否需要格式转换
    TRefCountPtr<IPooledRenderTarget> FinalRenderTarget = CaptureRenderTarget;
    
    if (ConversionRenderTarget.IsValid() && CaptureRenderTarget->GetFormat() != ConversionRenderTarget->GetFormat())
    {
        ConvertRenderTargetFormat(RHICmdList, CaptureRenderTarget, ConversionRenderTarget);
        FinalRenderTarget = ConversionRenderTarget;
    }
    
    // 创建帧数据
    FOmniCaptureRenderFrame Frame;
    Frame.RenderTarget = FinalRenderTarget;
    Frame.Fence = Fence;
    Frame.Timestamp = FPlatformTime::Seconds();
    Frame.bIsKeyFrame = (TotalFramesCaptured % 30 == 0); // 每30帧一个关键帧
    Frame.FrameIndex = TotalFramesCaptured;
    
    // 将帧加入队列
    CapturedFramesQueue.Enqueue(Frame);
    
    // 处理队列中的帧（在单独的线程中）
    // 这里简化处理，实际项目中应该使用工作线程
    ProcessCapturedFrameQueue();
    
    TotalFramesCaptured++;
    
    UE_LOG(LogOmniCaptureRenderer, Verbose, TEXT("Frame captured: Index=%lld, Timestamp=%.3f"), 
        Frame.FrameIndex, Frame.Timestamp);
}

void FOmniCaptureRenderer::ProcessCapturedFrame(const FOmniCaptureRenderFrame& Frame)
{
    if (!Encoder || !Encoder->IsInitialized())
    {
        UE_LOG(LogOmniCaptureRenderer, Error, TEXT("Cannot process frame: Encoder not initialized"));
        return;
    }
    
    // 等待GPU栅栏，确保帧数据可用
    WaitForGPUFence(Frame.Fence);
    
    // 将帧传递给编码器
    bool bSuccess = Encoder->EnqueueFrame(Frame.RenderTarget, Frame.Fence, Frame.Timestamp, Frame.bIsKeyFrame);
    
    if (!bSuccess)
    {
        UE_LOG(LogOmniCaptureRenderer, Error, TEXT("Failed to enqueue frame %lld to encoder"), Frame.FrameIndex);
    }
    else
    {
        // 处理已编码的帧
        Encoder->ProcessEncodedFrames([this](const uint8* Data, uint32 Size, double Timestamp, bool bIsKeyFrame) {
            HandleEncodedFrame(Data, Size, Timestamp, bIsKeyFrame);
        });
    }
}

bool FOmniCaptureRenderer::ShouldCaptureCurrentFrame() const
{
    if (!Config.bLimitFrameRate)
    {
        return true;
    }
    
    // 计算目标帧间隔
    double TargetFrameInterval = 1.0 / Config.CaptureFrequency;
    double CurrentTime = FPlatformTime::Seconds();
    double TimeSinceLastCapture = CurrentTime - LastCaptureTime;
    
    return TimeSinceLastCapture >= TargetFrameInterval;
}

void FOmniCaptureRenderer::UpdateFrameTiming()
{
    double CurrentTime = FPlatformTime::Seconds();
    LastCaptureTime = CurrentTime;
    
    // 更新帧时间累加器
    if (TotalFramesCaptured > 0)
    {
        static double LastUpdateTime = 0.0;
        if (CurrentTime - LastUpdateTime >= 1.0) // 每秒更新一次
        {
            double ActualFrameRate = TotalFramesCaptured / (CurrentTime - LastUpdateTime);
            UE_LOG(LogOmniCaptureRenderer, Log, TEXT("Capture frame rate: %.1f FPS"), ActualFrameRate);
            LastUpdateTime = CurrentTime;
            TotalFramesCaptured = 0;
        }
    }
}

FGPUFenceRHIRef FOmniCaptureRenderer::CreateGPUFence()
{
    return RHICreateGPUFence(TEXT("OmniCapture.FrameFence"));
}

void FOmniCaptureRenderer::WaitForGPUFence(const FGPUFenceRHIRef& Fence)
{
    if (Fence.IsValid())
    {
        // 等待GPU栅栏完成
        RHIGPUFenceWait(Fence, 0, FPlatformTime::Seconds() + 5.0); // 5秒超时
    }
}

void FOmniCaptureRenderer::ConvertRenderTargetFormat(FRHICommandListImmediate& RHICmdList, const TRefCountPtr<IPooledRenderTarget>& SourceRT, TRefCountPtr<IPooledRenderTarget>& DestRT)
{
    if (!SourceRT.IsValid() || !DestRT.IsValid())
    {
        UE_LOG(LogOmniCaptureRenderer, Error, TEXT("Invalid render targets for format conversion"));
        return;
    }
    
    // 这里简化了格式转换逻辑
    // 实际项目中应该使用着色器进行更复杂的格式转换
    const FTexture2DRHIRef& SrcTexture = SourceRT->GetRenderTargetItem().ShaderResourceTexture->GetTexture2D();
    const FTexture2DRHIRef& DestTexture = DestRT->GetRenderTargetItem().TargetableTexture->GetTexture2D();
    
    // 设置渲染目标
    RHICmdList.SetRenderTarget(0, DestRT->GetRenderTargetItem().TargetableTexture, FTextureRHIRef());
    
    // 清除渲染目标
    RHICmdList.Clear(true, FLinearColor::Black, false, 1.0f, false, 0, FIntRect());
    
    // 复制纹理（实际项目中应该使用格式转换着色器）
    RHICmdList.CopyTexture(
        SrcTexture, 0, 0,
        DestTexture, 0, 0,
        FIntPoint::ZeroValue,
        SrcTexture->GetSizeXYZ().XY()
    );
    
    UE_LOG(LogOmniCaptureRenderer, Verbose, TEXT("Format conversion from %d to %d"), 
        SourceRT->GetFormat(), DestRT->GetFormat());
}

void FOmniCaptureRenderer::HandleEncodedFrame(const uint8* Data, uint32 Size, double Timestamp, bool bIsKeyFrame)
{
    // 这里处理编码后的帧数据
    // 实际项目中应该将数据写入文件或通过网络发送
    UE_LOG(LogOmniCaptureRenderer, Verbose, TEXT("Encoded frame received: Size=%u, Timestamp=%.3f, KeyFrame=%d"), 
        Size, Timestamp, bIsKeyFrame ? 1 : 0);
    
    // 这里可以添加文件写入逻辑
}

void FOmniCaptureRenderer::ProcessCapturedFrameQueue()
{
    // 处理队列中的所有帧
    FOmniCaptureRenderFrame Frame;
    while (CapturedFramesQueue.Dequeue(Frame))
    {
        ProcessCapturedFrame(Frame);
    }
}

// 渲染事件处理函数
void FOmniCaptureRenderer::OnPostResolvedSceneColor(FRHICommandListImmediate& RHICmdList, FSceneRenderTargets& SceneContext, const FSceneView& View)
{
    ProcessFrame_RenderThread(RHICmdList, View);
}

// UOmniCaptureRenderComponent实现
UOmniCaptureRenderComponent::UOmniCaptureRenderComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
    , CaptureResolution(1920, 1080)
    , bCaptureHDR(false)
    , bCaptureAlpha(false)
    , CaptureFrameRate(60.0f)
    , bLimitFrameRate(true)
    , OutputFormat(EOmniOutputFormat::NVENCHardware)
    , bAutoStartCapture(false)
    , Renderer(nullptr)
    , Encoder(nullptr)
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UOmniCaptureRenderComponent::BeginPlay()
{
    Super::BeginPlay();
    
    // 初始化渲染器
    InitializeRenderer();
    
    // 如果设置了自动开始捕获，则开始捕获
    if (bAutoStartCapture)
    {
        StartCapture();
    }
}

void UOmniCaptureRenderComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    // 这里可以添加额外的组件逻辑
}

void UOmniCaptureRenderComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 停止捕获并清理资源
    StopCapture();
    
    if (Renderer.IsValid())
    {
        Renderer->Shutdown();
        Renderer.Reset();
    }
    
    Encoder.Reset();
    
    Super::EndPlay(EndPlayReason);
}

void UOmniCaptureRenderComponent::OnRegister()
{
    Super::OnRegister();
}

void UOmniCaptureRenderComponent::OnUnregister()
{
    // 停止捕获并清理资源
    StopCapture();
    
    if (Renderer.IsValid())
    {
        Renderer->Shutdown();
        Renderer.Reset();
    }
    
    Encoder.Reset();
    
    Super::OnUnregister();
}

void UOmniCaptureRenderComponent::StartCapture()
{
    if (!Renderer.IsValid())
    {
        HandleError(TEXT("Renderer not initialized"));
        return;
    }
    
    Renderer->StartCapture();
    ReceiveCaptureStarted();
}

void UOmniCaptureRenderComponent::StopCapture()
{
    if (Renderer.IsValid())
    {
        Renderer->StopCapture();
    }
    ReceiveCaptureStopped();
}

void UOmniCaptureRenderComponent::PauseCapture()
{
    if (Renderer.IsValid())
    {
        Renderer->PauseCapture();
    }
}

void UOmniCaptureRenderComponent::ResumeCapture()
{
    if (Renderer.IsValid())
    {
        Renderer->ResumeCapture();
    }
}

void UOmniCaptureRenderComponent::SetResolution(FIntPoint NewResolution)
{
    CaptureResolution = NewResolution;
    
    if (Renderer.IsValid())
    {
        FOmniCaptureRendererConfig Config = Renderer->GetConfig();
        Config.Resolution = NewResolution;
        Renderer->UpdateConfig(Config);
    }
}

void UOmniCaptureRenderComponent::SetCaptureFrameRate(float NewFrameRate)
{
    CaptureFrameRate = FMath::Clamp(NewFrameRate, 1.0f, 120.0f);
    
    if (Renderer.IsValid())
    {
        FOmniCaptureRendererConfig Config = Renderer->GetConfig();
        Config.CaptureFrequency = CaptureFrameRate;
        Renderer->UpdateConfig(Config);
    }
}

void UOmniCaptureRenderComponent::SetCaptureHDR(bool NewCaptureHDR)
{
    bCaptureHDR = NewCaptureHDR;
    
    if (Renderer.IsValid())
    {
        FOmniCaptureRendererConfig Config = Renderer->GetConfig();
        Config.bCaptureHDR = bCaptureHDR;
        Renderer->UpdateConfig(Config);
    }
}

void UOmniCaptureRenderComponent::SetOutputFormat(EOmniOutputFormat NewFormat)
{
    OutputFormat = NewFormat;
    
    // 如果已经初始化，需要重新创建编码器
    if (Renderer.IsValid())
    {
        // 停止当前捕获
        bool bWasCapturing = Renderer->IsCapturing();
        Renderer->StopCapture();
        
        // 创建新编码器
        Encoder = CreateEncoder();
        
        // 设置新编码器
        if (Encoder.IsValid())
        {
            Renderer->SetEncoder(Encoder);
            
            // 如果之前在捕获，重新开始
            if (bWasCapturing)
            {
                Renderer->StartCapture();
            }
        }
        else
        {
            HandleError(TEXT("Failed to create encoder for new format"));
        }
    }
}

bool UOmniCaptureRenderComponent::IsCapturing() const
{
    return Renderer.IsValid() && Renderer->IsCapturing();
}

bool UOmniCaptureRenderComponent::IsPaused() const
{
    return Renderer.IsValid() && Renderer->IsPaused();
}

FOmniCaptureRendererConfig UOmniCaptureRenderComponent::GetCurrentConfig() const
{
    if (Renderer.IsValid())
    {
        return Renderer->GetConfig();
    }
    
    FOmniCaptureRendererConfig Config;
    Config.Resolution = CaptureResolution;
    Config.bCaptureHDR = bCaptureHDR;
    Config.bCaptureAlpha = bCaptureAlpha;
    Config.CaptureFrequency = CaptureFrameRate;
    Config.bLimitFrameRate = bLimitFrameRate;
    
    return Config;
}

void UOmniCaptureRenderComponent::InitializeRenderer()
{
    // 创建编码器
    Encoder = CreateEncoder();
    if (!Encoder.IsValid())
    {
        HandleError(TEXT("Failed to create encoder"));
        return;
    }
    
    // 创建渲染器配置
    FOmniCaptureRendererConfig Config;
    Config.Resolution = CaptureResolution;
    Config.bCaptureHDR = bCaptureHDR;
    Config.bCaptureAlpha = bCaptureAlpha;
    Config.CaptureFrequency = CaptureFrameRate;
    Config.bLimitFrameRate = bLimitFrameRate;
    
    // 创建并初始化渲染器
    Renderer = MakeShareable(new FOmniCaptureRenderer());
    if (!Renderer->Initialize(Config, Encoder))
    {
        HandleError(TEXT("Failed to initialize renderer"));
        Renderer.Reset();
        Encoder.Reset();
    }
}

TSharedPtr<IOmniCaptureEncoder> UOmniCaptureRenderComponent::CreateEncoder()
{
    // 使用编码器工厂创建编码器
    return FOmniCaptureEncoderFactory::CreateEncoder(OutputFormat);
}

void UOmniCaptureRenderComponent::HandleError(const FString& ErrorMessage)
{
    UE_LOG(LogOmniCaptureRenderer, Error, TEXT("Capture error: %s"), *ErrorMessage);
    ReceiveCaptureError(ErrorMessage);
}