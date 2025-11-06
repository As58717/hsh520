// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OmniCaptureTypes.h"
#include "Containers/Queue.h"
#include "RHI.h"
#include "RHIResources.h"
#include "Templates/SharedPointer.h"

class FNVENCEncoderSession;
class FNVENCFrameContext;

/**
 * 硬件能力结构体，记录NVENC硬件支持的编码能力
 */
struct FOmniNVENCDirectCapabilities
{
    bool bIsSupported = false;
    bool bSupportsH264 = false;
    bool bSupportsHEVC = false;
    bool bSupportsAV1 = false;
    bool bSupportsNV12 = false;
    bool bSupportsP010 = false;
    bool bSupportsBGRA = false;
    int32 MaxWidth = 0;
    int32 MaxHeight = 0;
    int32 MaxBitrateKbps = 0;
    FString DeviceName;
    FString DriverVersion;
    int32 SDKVersion = 0;
};

/**
 * NVENC直接编码器接口
 */
class OMNICAPTURE_API FOmniCaptureNVENCEncoderDirect : public TSharedFromThis<FOmniCaptureNVENCEncoderDirect>
{
public:
    FOmniCaptureNVENCEncoderDirect();
    virtual ~FOmniCaptureNVENCEncoderDirect();

    /**
     * 初始化编码器
     */
    bool Initialize(const FIntPoint& InResolution, EOmniCaptureCodec InCodec, const FOmniCaptureQuality& InQuality, EOmniCaptureColorFormat InColorFormat);

    /**
     * 关闭编码器
     */
    void Shutdown();

    /**
     * 将帧入队进行编码
     */
    bool EnqueueFrame(const TRefCountPtr<IPooledRenderTarget>& InRenderTarget, const FGPUFenceRHIRef& InFence, double InTimestamp, bool bIsKeyFrame = false);

    /**
     * 将CPU纹理入队进行编码（回退路径）
     */
    bool EnqueueCPUBuffer(const void* InBuffer, const FIntPoint& InResolution, EPixelFormat InFormat, double InTimestamp, bool bIsKeyFrame = false);

    /**
     * 处理编码的帧数据
     */
    bool ProcessEncodedFrames(TFunctionRef<void(const uint8*, uint32, double, bool)> OnFrameEncoded);

    /**
     * 最终化编码器，处理所有剩余帧
     */
    void Finalize(TFunctionRef<void(const uint8*, uint32, double, bool)> OnFrameEncoded);

    /**
     * 检查NVENC是否可用
     */
    static bool IsNVENCAvailable();

    /**
     * 获取NVENC硬件能力
     */
    static FOmniNVENCDirectCapabilities GetNVENCCapabilities();

    /**
     * 检查系统是否支持指定的编码配置
     */
    static bool IsConfigurationSupported(EOmniCaptureCodec InCodec, EOmniCaptureColorFormat InColorFormat, const FIntPoint& InResolution);

    /**
     * 获取编码器状态
     */
    bool IsInitialized() const { return bIsInitialized; }

    /**
     * 获取编码器统计信息
     */
    struct FStats
    {
        int32 EncodedFrames = 0;
        int32 DroppedFrames = 0;
        double AverageEncodeTimeMs = 0.0;
    };
    FStats GetStats() const { return Stats; }

private:
    /**
     * 初始化NVENC API
     */
    bool InitializeNVENCAPI();

    /**
     * 关闭NVENC API
     */
    void ShutdownNVENCAPI();

    /**
     * 创建编码器会话
     */
    bool CreateEncoderSession(const FIntPoint& InResolution, EOmniCaptureCodec InCodec, const FOmniCaptureQuality& InQuality, EOmniCaptureColorFormat InColorFormat);

    /**
     * 清理编码器会话
     */
    void CleanupEncoderSession();

    /**
     * 处理队列中的帧
     */
    void ProcessFrameQueue();

private:
    // 状态标志
    bool bIsInitialized = false;
    bool bIsNVENCAPIInitialized = false;

    // 编码器会话
    TUniquePtr<FNVENCEncoderSession> EncoderSession;

    // 帧队列
    TQueue<TSharedPtr<FNVENCFrameContext>> FrameQueue;
    FCriticalSection FrameQueueMutex;

    // 编码线程
    FRunnableThread* EncodeThread = nullptr;
    TAtomic<bool> bEncodeThreadShouldExit = false;

    // 统计信息
    FStats Stats;
    TArray<double> EncodeTimes;
    FCriticalSection StatsMutex;

    // 静态NVENC API状态
    static void* NVENCModuleHandle;
    static int32 NVENCReferenceCount;
    static FCriticalSection NVENCModuleMutex;
};