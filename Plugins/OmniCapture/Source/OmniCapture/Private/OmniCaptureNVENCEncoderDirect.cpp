// Copyright Epic Games, Inc. All Rights Reserved.

#include "OmniCaptureNVENCEncoderDirect.h"
#include "NVENCEncoderSession.h"
#include "Logging/LogMacros.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HAL/Runnable.h"

// 定义日志类别
DECLARE_LOG_CATEGORY_EXTERN(LogOmniCaptureNVENC, Log, All);

// 静态成员初始化
void* FOmniCaptureNVENCEncoderDirect::NVENCModuleHandle = nullptr;
int32 FOmniCaptureNVENCEncoderDirect::NVENCReferenceCount = 0;
FCriticalSection FOmniCaptureNVENCEncoderDirect::NVENCModuleMutex;

// 编码线程类
class FNVENCEncodeThread : public FRunnable
{
public:
    FNVENCEncodeThread(FOmniCaptureNVENCEncoderDirect* InEncoder)
        : Encoder(InEncoder)
        , Thread(nullptr)
        , StopTaskCounter(0)
    {
    }

    virtual ~FNVENCEncodeThread() override
    {
        Stop();
    }

    virtual uint32 Run() override
    {
        while (StopTaskCounter.GetValue() == 0)
        {
            if (Encoder)
            {
                Encoder->ProcessFrameQueue();
            }
            FPlatformProcess::Sleep(0.001f); // 1ms sleep to avoid busy waiting
        }
        return 0;
    }

    void Start()
    {
        if (!Thread)
        {
            Thread = FRunnableThread::Create(this, TEXT("NVENCEncodeThread"), 0, TPri_Normal);
        }
    }

    void Stop()
    {
        StopTaskCounter.Increment();
        if (Thread)
        {
            Thread->WaitForCompletion();
            delete Thread;
            Thread = nullptr;
        }
    }

private:
    FOmniCaptureNVENCEncoderDirect* Encoder;
    FRunnableThread* Thread;
    FThreadSafeCounter StopTaskCounter;
};

FOmniCaptureNVENCEncoderDirect::FOmniCaptureNVENCEncoderDirect()
    : bIsInitialized(false)
    , bIsNVENCAPIInitialized(false)
    , bEncodeThreadShouldExit(false)
    , EncodeThread(nullptr)
{
    UE_LOG(LogOmniCaptureNVENC, Log, TEXT("Creating NVENC direct encoder"));
}

FOmniCaptureNVENCEncoderDirect::~FOmniCaptureNVENCEncoderDirect()
{
    Shutdown();
    UE_LOG(LogOmniCaptureNVENC, Log, TEXT("NVENC direct encoder destroyed"));
}

bool FOmniCaptureNVENCEncoderDirect::Initialize(const FIntPoint& InResolution, EOmniCaptureCodec InCodec, const FOmniCaptureQuality& InQuality, EOmniCaptureColorFormat InColorFormat)
{
    UE_LOG(LogOmniCaptureNVENC, Log, TEXT("Initializing NVENC direct encoder with resolution %dx%d, codec %d, color format %d"), 
           InResolution.X, InResolution.Y, (int32)InCodec, (int32)InColorFormat);

    // 检查NVENC是否可用
    if (!IsNVENCAvailable())
    {
        UE_LOG(LogOmniCaptureNVENC, Error, TEXT("NVENC is not available"));
        return false;
    }

    // 检查配置是否支持
    if (!IsConfigurationSupported(InCodec, InColorFormat, InResolution))
    {
        UE_LOG(LogOmniCaptureNVENC, Error, TEXT("Configuration is not supported by NVENC"));
        return false;
    }

    // 初始化NVENC API
    if (!InitializeNVENCAPI())
    {
        UE_LOG(LogOmniCaptureNVENC, Error, TEXT("Failed to initialize NVENC API"));
        return false;
    }

    // 创建编码器会话
    if (!CreateEncoderSession(InResolution, InCodec, InQuality, InColorFormat))
    {
        UE_LOG(LogOmniCaptureNVENC, Error, TEXT("Failed to create encoder session"));
        ShutdownNVENCAPI();
        return false;
    }

    // 创建编码线程
    EncodeThread = new FNVENCEncodeThread(this);
    if (EncodeThread)
    {
        EncodeThread->Start();
    }

    bIsInitialized = true;
    UE_LOG(LogOmniCaptureNVENC, Log, TEXT("NVENC direct encoder initialized successfully"));
    return true;
}

void FOmniCaptureNVENCEncoderDirect::Shutdown()
{
    if (!bIsInitialized)
    {
        return;
    }

    // 停止编码线程
    bEncodeThreadShouldExit = true;
    if (EncodeThread)
    {
        delete EncodeThread;
        EncodeThread = nullptr;
    }

    // 清理编码器会话
    CleanupEncoderSession();

    // 关闭NVENC API
    ShutdownNVENCAPI();

    // 清空队列
    FrameQueueMutex.Lock();
    TSharedPtr<FNVENCFrameContext> FrameContext;
    while (FrameQueue.Dequeue(FrameContext))
    {
        // 释放帧资源
        StatsMutex.Lock();
        Stats.DroppedFrames++;
        StatsMutex.Unlock();
    }
    FrameQueueMutex.Unlock();

    bIsInitialized = false;
    UE_LOG(LogOmniCaptureNVENC, Log, TEXT("NVENC direct encoder shut down"));
}

bool FOmniCaptureNVENCEncoderDirect::EnqueueFrame(const TRefCountPtr<IPooledRenderTarget>& InRenderTarget, const FGPUFenceRHIRef& InFence, double InTimestamp, bool bIsKeyFrame)
{
    if (!bIsInitialized || !InRenderTarget.IsValid())
    {
        return false;
    }

    // 创建帧上下文
    TSharedPtr<FNVENCFrameContext> FrameContext = MakeShareable(new FNVENCFrameContext());
    FrameContext->RenderTarget = InRenderTarget;
    FrameContext->Fence = InFence;
    FrameContext->Timestamp = InTimestamp;
    FrameContext->bIsKeyFrame = bIsKeyFrame;
    FrameContext->bIsCPUFrame = false;

    // 入队
    FrameQueueMutex.Lock();
    bool bEnqueued = FrameQueue.Enqueue(FrameContext);
    FrameQueueMutex.Unlock();

    if (!bEnqueued)
    {
        StatsMutex.Lock();
        Stats.DroppedFrames++;
        StatsMutex.Unlock();
        UE_LOG(LogOmniCaptureNVENC, Warning, TEXT("Failed to enqueue frame"));
    }

    return bEnqueued;
}

bool FOmniCaptureNVENCEncoderDirect::EnqueueCPUBuffer(const void* InBuffer, const FIntPoint& InResolution, EPixelFormat InFormat, double InTimestamp, bool bIsKeyFrame)
{
    if (!bIsInitialized || !InBuffer)
    {
        return false;
    }

    // 创建CPU帧上下文
    TSharedPtr<FNVENCFrameContext> FrameContext = MakeShareable(new FNVENCFrameContext());
    
    // 复制CPU缓冲区数据
    uint32 BufferSize = 0;
    switch (InFormat)
    {
        case PF_B8G8R8A8:
            BufferSize = InResolution.X * InResolution.Y * 4;
            break;
        case PF_FloatRGBA:
            BufferSize = InResolution.X * InResolution.Y * 16;
            break;
        // 添加其他格式的支持
        default:
            UE_LOG(LogOmniCaptureNVENC, Error, TEXT("Unsupported pixel format for CPU encoding: %d"), (int32)InFormat);
            return false;
    }

    FrameContext->CPUBuffer.SetNum(BufferSize);
    FMemory::Memcpy(FrameContext->CPUBuffer.GetData(), InBuffer, BufferSize);
    FrameContext->CPUResolution = InResolution;
    FrameContext->CPUFormat = InFormat;
    FrameContext->Timestamp = InTimestamp;
    FrameContext->bIsKeyFrame = bIsKeyFrame;
    FrameContext->bIsCPUFrame = true;

    // 入队
    FrameQueueMutex.Lock();
    bool bEnqueued = FrameQueue.Enqueue(FrameContext);
    FrameQueueMutex.Unlock();

    if (!bEnqueued)
    {
        StatsMutex.Lock();
        Stats.DroppedFrames++;
        StatsMutex.Unlock();
        UE_LOG(LogOmniCaptureNVENC, Warning, TEXT("Failed to enqueue CPU buffer"));
    }

    return bEnqueued;
}

bool FOmniCaptureNVENCEncoderDirect::ProcessEncodedFrames(TFunctionRef<void(const uint8*, uint32, double, bool)> OnFrameEncoded)
{
    if (!bIsInitialized)
    {
        return false;
    }

    // 实际实现中，这里应该处理编码后的帧数据
    // 由于我们使用了单独的编码线程，这里可能需要从输出队列获取已编码的帧
    UE_LOG(LogOmniCaptureNVENC, Warning, TEXT("ProcessEncodedFrames: Simplified implementation"));
    
    return true;
}

void FOmniCaptureNVENCEncoderDirect::Finalize(TFunctionRef<void(const uint8*, uint32, double, bool)> OnFrameEncoded)
{
    if (!bIsInitialized)
    {
        return;
    }

    // 停止编码线程
    bEncodeThreadShouldExit = true;
    if (EncodeThread)
    {
        delete EncodeThread;
        EncodeThread = nullptr;
    }

    // 处理剩余的帧
    ProcessFrameQueue();

    // 刷新编码器
    if (EncoderSession)
    {
        TArray<uint8> Bitstream;
        while (EncoderSession->Flush(Bitstream))
        {
            if (Bitstream.Num() > 0)
            {
                OnFrameEncoded(Bitstream.GetData(), Bitstream.Num(), 0.0, true);
            }
        }
    }

    UE_LOG(LogOmniCaptureNVENC, Log, TEXT("NVENC encoder finalized"));
}

bool FOmniCaptureNVENCEncoderDirect::IsNVENCAvailable()
{
    // 检查NVENC模块是否存在
    FString NVENCDllPath = TEXT("nvEncodeAPI64.dll");
    FString SystemPath = FPlatformMisc::GetWindowsSystemDir();
    FString FullNVENCDllPath = FPaths::Combine(SystemPath, NVENCDllPath);
    
    return FPaths::FileExists(FullNVENCDllPath) || FPlatformProcess::GetDllHandle(*NVENCDllPath) != nullptr;
}

FOmniNVENCDirectCapabilities FOmniCaptureNVENCEncoderDirect::GetNVENCCapabilities()
{
    FOmniNVENCDirectCapabilities Capabilities;
    
    if (!IsNVENCAvailable())
    {
        Capabilities.bIsSupported = false;
        return Capabilities;
    }

    Capabilities.bIsSupported = true;
    
    // 获取NVENC编码器能力
    auto NVENCCapabilities = FNVENCEncoderSession::GetCapabilities();
    Capabilities.bSupportsH264 = NVENCCapabilities.bSupportsH264;
    Capabilities.bSupportsHEVC = NVENCCapabilities.bSupportsHEVC;
    Capabilities.bSupportsAV1 = NVENCCapabilities.bSupportsAV1;
    Capabilities.bSupportsNV12 = NVENCCapabilities.bSupportsNV12;
    Capabilities.bSupportsP010 = NVENCCapabilities.bSupportsP010;
    Capabilities.bSupportsBGRA = NVENCCapabilities.bSupportsBGRA;
    Capabilities.MaxWidth = NVENCCapabilities.MaxWidth;
    Capabilities.MaxHeight = NVENCCapabilities.MaxHeight;
    Capabilities.MaxBitrateKbps = NVENCCapabilities.MaxBitrateKbps;
    
    // 获取设备信息
    Capabilities.DeviceName = TEXT("NVIDIA GPU");
    Capabilities.DriverVersion = TEXT("Unknown");
    Capabilities.SDKVersion = 0;
    
    return Capabilities;
}

bool FOmniCaptureNVENCEncoderDirect::IsConfigurationSupported(EOmniCaptureCodec InCodec, EOmniCaptureColorFormat InColorFormat, const FIntPoint& InResolution)
{
    return FNVENCEncoderSession::IsConfigurationSupported(InCodec, InColorFormat, InResolution);
}

bool FOmniCaptureNVENCEncoderDirect::InitializeNVENCAPI()
{
    NVENCModuleMutex.Lock();
    
    if (NVENCReferenceCount == 0)
    {
        // 首次初始化
        UE_LOG(LogOmniCaptureNVENC, Log, TEXT("Initializing NVENC API (reference count: %d)"), NVENCReferenceCount);
        // 实际实现中，这里可能需要加载NVENC模块和初始化全局API
    }
    
    NVENCReferenceCount++;
    bIsNVENCAPIInitialized = true;
    
    NVENCModuleMutex.Unlock();
    return true;
}

void FOmniCaptureNVENCEncoderDirect::ShutdownNVENCAPI()
{
    if (!bIsNVENCAPIInitialized)
    {
        return;
    }

    NVENCModuleMutex.Lock();
    
    NVENCReferenceCount--;
    
    if (NVENCReferenceCount == 0)
    {
        // 最后一个引用，清理API
        UE_LOG(LogOmniCaptureNVENC, Log, TEXT("Shutting down NVENC API (reference count: %d)"), NVENCReferenceCount);
        // 实际实现中，这里可能需要卸载NVENC模块和清理全局API
    }
    
    bIsNVENCAPIInitialized = false;
    
    NVENCModuleMutex.Unlock();
}

bool FOmniCaptureNVENCEncoderDirect::CreateEncoderSession(const FIntPoint& InResolution, EOmniCaptureCodec InCodec, const FOmniCaptureQuality& InQuality, EOmniCaptureColorFormat InColorFormat)
{
    EncoderSession = MakeUnique<FNVENCEncoderSession>();
    if (!EncoderSession)
    {
        return false;
    }

    return EncoderSession->Initialize(InResolution, InCodec, InQuality, InColorFormat);
}

void FOmniCaptureNVENCEncoderDirect::CleanupEncoderSession()
{
    if (EncoderSession)
    {
        EncoderSession->Shutdown();
        EncoderSession.Reset();
    }
}

void FOmniCaptureNVENCEncoderDirect::ProcessFrameQueue()
{
    if (!bIsInitialized || !EncoderSession)
    {
        return;
    }

    // 从队列获取帧
    TSharedPtr<FNVENCFrameContext> FrameContext;
    FrameQueueMutex.Lock();
    bool bHasFrame = FrameQueue.Dequeue(FrameContext);
    FrameQueueMutex.Unlock();

    if (!bHasFrame || !FrameContext)
    {
        return;
    }

    // 记录开始时间
    double StartTime = FPlatformTime::Seconds();
    
    // 处理帧
    TArray<uint8> Bitstream;
    bool bEncoded = false;
    
    if (FrameContext->bIsCPUFrame)
    {
        // CPU帧路径
        bEncoded = EncoderSession->EncodeBuffer(
            FrameContext->CPUBuffer.GetData(),
            FrameContext->CPUResolution,
            FrameContext->CPUFormat,
            FrameContext->Timestamp,
            FrameContext->bIsKeyFrame,
            Bitstream
        );
    }
    else
    {
        // GPU帧路径
        if (FrameContext->Fence.IsValid())
        {
            // 等待GPU完成
            FrameContext->Fence->Wait();
        }
        
        if (FrameContext->RenderTarget.IsValid() && FrameContext->RenderTarget->GetRenderTargetTexture().IsValid())
        {
            bEncoded = EncoderSession->EncodeTexture(
                FrameContext->RenderTarget->GetRenderTargetTexture(),
                FrameContext->Timestamp,
                FrameContext->bIsKeyFrame,
                Bitstream
            );
        }
    }

    // 记录结束时间
    double EncodeTimeMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;
    
    // 更新统计信息
    StatsMutex.Lock();
    if (bEncoded)
    {
        Stats.EncodedFrames++;
        EncodeTimes.Add(EncodeTimeMs);
        if (EncodeTimes.Num() > 100)
        {
            EncodeTimes.RemoveAt(0);
        }
        
        // 计算平均编码时间
        double TotalEncodeTime = 0.0;
        for (double Time : EncodeTimes)
        {
            TotalEncodeTime += Time;
        }
        Stats.AverageEncodeTimeMs = TotalEncodeTime / EncodeTimes.Num();
    }
    else
    {
        Stats.DroppedFrames++;
    }
    StatsMutex.Unlock();
}