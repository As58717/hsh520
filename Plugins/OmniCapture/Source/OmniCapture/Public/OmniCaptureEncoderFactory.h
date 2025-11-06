// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OmniCaptureTypes.h"
#include "Templates/SharedPointer.h"

class FOmniCaptureNVENCEncoderDirect;
class FOmniCaptureImageWriter;

/**
 * 编码器接口基类
 */
class IOmniCaptureEncoder : public TSharedFromThis<IOmniCaptureEncoder>
{
public:
    virtual ~IOmniCaptureEncoder() {}

    /**
     * 初始化编码器
     */
    virtual bool Initialize(const FIntPoint& InResolution, EOmniCaptureCodec InCodec, const FOmniCaptureQuality& InQuality, EOmniCaptureColorFormat InColorFormat) = 0;

    /**
     * 关闭编码器
     */
    virtual void Shutdown() = 0;

    /**
     * 将GPU帧入队编码
     */
    virtual bool EnqueueFrame(const TRefCountPtr<IPooledRenderTarget>& InRenderTarget, const FGPUFenceRHIRef& InFence, double InTimestamp, bool bIsKeyFrame = false) = 0;

    /**
     * 将CPU缓冲区入队编码
     */
    virtual bool EnqueueCPUBuffer(const void* InBuffer, const FIntPoint& InResolution, EPixelFormat InFormat, double InTimestamp, bool bIsKeyFrame = false) = 0;

    /**
     * 处理编码的帧
     */
    virtual bool ProcessEncodedFrames(TFunctionRef<void(const uint8*, uint32, double, bool)> OnFrameEncoded) = 0;

    /**
     * 最终化编码器
     */
    virtual void Finalize(TFunctionRef<void(const uint8*, uint32, double, bool)> OnFrameEncoded) = 0;

    /**
     * 检查编码器是否初始化
     */
    virtual bool IsInitialized() const = 0;

    /**
     * 获取编码器类型
     */
    virtual EOmniOutputFormat GetEncoderType() const = 0;
};

/**
 * 编码器工厂，用于创建和管理编码器实例
 */
class OMNICAPTURE_API FOmniCaptureEncoderFactory
{
public:
    /**
     * 创建编码器实例
     */
    static TSharedPtr<IOmniCaptureEncoder> CreateEncoder(EOmniOutputFormat InFormat);

    /**
     * 检查指定输出格式是否可用
     */
    static bool IsOutputFormatAvailable(EOmniOutputFormat InFormat);

    /**
     * 获取所有可用的输出格式
     */
    static TArray<EOmniOutputFormat> GetAvailableOutputFormats();

    /**
     * 获取指定格式的编码器信息
     */
    struct FEncoderInfo
    {
        FString DisplayName;
        FString Description;
        bool bIsHardwareAccelerated;
        bool bSupportsRealtime;
        TArray<EOmniCaptureCodec> SupportedCodecs;
        TArray<EOmniCaptureColorFormat> SupportedColorFormats;
    };
    static FEncoderInfo GetEncoderInfo(EOmniOutputFormat InFormat);

    /**
     * 获取推荐的编码器配置
     */
    static bool GetRecommendedConfiguration(EOmniOutputFormat InFormat, EOmniCaptureCodec& OutCodec, EOmniCaptureColorFormat& OutColorFormat);

    /**
     * 注册自定义编码器（扩展点）
     */
    using EncoderFactoryFunc = TFunction<TSharedPtr<IOmniCaptureEncoder>()>;
    static void RegisterCustomEncoder(EOmniOutputFormat InFormat, EncoderFactoryFunc InFactoryFunc);

    /**
     * 取消注册自定义编码器
     */
    static void UnregisterCustomEncoder(EOmniOutputFormat InFormat);
};

/**
 * 直接NVENC编码器的实现适配器
 */
class FOmniCaptureDirectNVENCAdapter : public IOmniCaptureEncoder
{
public:
    FOmniCaptureDirectNVENCAdapter();
    virtual ~FOmniCaptureDirectNVENCAdapter() override;

    // IOmniCaptureEncoder接口实现
    virtual bool Initialize(const FIntPoint& InResolution, EOmniCaptureCodec InCodec, const FOmniCaptureQuality& InQuality, EOmniCaptureColorFormat InColorFormat) override;
    virtual void Shutdown() override;
    virtual bool EnqueueFrame(const TRefCountPtr<IPooledRenderTarget>& InRenderTarget, const FGPUFenceRHIRef& InFence, double InTimestamp, bool bIsKeyFrame = false) override;
    virtual bool EnqueueCPUBuffer(const void* InBuffer, const FIntPoint& InResolution, EPixelFormat InFormat, double InTimestamp, bool bIsKeyFrame = false) override;
    virtual bool ProcessEncodedFrames(TFunctionRef<void(const uint8*, uint32, double, bool)> OnFrameEncoded) override;
    virtual void Finalize(TFunctionRef<void(const uint8*, uint32, double, bool)> OnFrameEncoded) override;
    virtual bool IsInitialized() const override;
    virtual EOmniOutputFormat GetEncoderType() const override;

private:
    TSharedPtr<FOmniCaptureNVENCEncoderDirect> Encoder;
};