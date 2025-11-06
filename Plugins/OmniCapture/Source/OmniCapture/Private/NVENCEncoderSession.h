// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OmniCaptureTypes.h"
#include "RHI.h"
#include "RHIResources.h"

// NVENC API类型定义（避免直接包含NVIDIA头文件）
typedef struct _NV_ENCODE_API_FUNCTION_LIST* PNV_ENCODE_API_FUNCTION_LIST;
typedef void* NV_ENC_DEVICE;
typedef void* NV_ENC_ENCODER;
typedef void* NV_ENC_INPUT_RESOURCE;
typedef void* NV_ENC_OUTPUT_PTR;

/**
 * NVENC编码器会话类，封装NVENC API的使用
 */
class FNVENCEncoderSession
{
public:
    FNVENCEncoderSession();
    ~FNVENCEncoderSession();

    /**
     * 初始化编码器会话
     */
    bool Initialize(const FIntPoint& InResolution, EOmniCaptureCodec InCodec, const FOmniCaptureQuality& InQuality, EOmniCaptureColorFormat InColorFormat);

    /**
     * 关闭编码器会话
     */
    void Shutdown();

    /**
     * 编码GPU纹理
     */
    bool EncodeTexture(const FTextureRHIRef& InTexture, double InTimestamp, bool bIsKeyFrame, TArray<uint8>& OutBitstream);

    /**
     * 编码CPU缓冲区
     */
    bool EncodeBuffer(const void* InBuffer, const FIntPoint& InResolution, EPixelFormat InFormat, double InTimestamp, bool bIsKeyFrame, TArray<uint8>& OutBitstream);

    /**
     * 刷新编码器，获取所有剩余帧
     */
    bool Flush(TArray<uint8>& OutBitstream);

    /**
     * 检查是否支持指定的配置
     */
    static bool IsConfigurationSupported(EOmniCaptureCodec InCodec, EOmniCaptureColorFormat InColorFormat, const FIntPoint& InResolution);

    /**
     * 获取编码器能力
     */
    struct FCapabilities
    {
        bool bSupportsH264 = false;
        bool bSupportsHEVC = false;
        bool bSupportsAV1 = false;
        bool bSupportsNV12 = false;
        bool bSupportsP010 = false;
        bool bSupportsBGRA = false;
        int32 MaxWidth = 0;
        int32 MaxHeight = 0;
        int32 MaxBitrateKbps = 0;
    };
    static FCapabilities GetCapabilities();

private:
    /**
     * 加载NVENC API函数
     */
    bool LoadNVENCApi();

    /**
     * 创建NVENC设备
     */
    bool CreateNVENCDevice();

    /**
     * 创建编码器
     */
    bool CreateEncoder(const FIntPoint& InResolution, EOmniCaptureCodec InCodec, const FOmniCaptureQuality& InQuality, EOmniCaptureColorFormat InColorFormat);

    /**
     * 分配编码资源
     */
    bool AllocateEncodingResources(const FIntPoint& InResolution, EOmniCaptureColorFormat InColorFormat);

    /**
     * 释放编码资源
     */
    void ReleaseEncodingResources();

    /**
     * 将UE纹理转换为NVENC输入格式
     */
    bool ConvertTextureToNVENCInput(const FTextureRHIRef& InTexture, NV_ENC_INPUT_RESOURCE& OutInputResource);

    /**
     * 设置编码参数
     */
    bool SetupEncodeParams(const FOmniCaptureQuality& InQuality, EOmniCaptureCodec InCodec);

    /**
     * 执行编码
     */
    bool DoEncode(bool bIsKeyFrame, TArray<uint8>& OutBitstream);

private:
    // NVENC API相关
    void* NVENCModuleHandle = nullptr;
    PNV_ENCODE_API_FUNCTION_LIST NvEncodeAPI = nullptr;
    NV_ENC_DEVICE NvEncDevice = nullptr;
    NV_ENC_ENCODER NvEncoder = nullptr;

    // 编码配置
    FIntPoint Resolution;
    EOmniCaptureCodec Codec;
    EOmniCaptureColorFormat ColorFormat;
    FOmniCaptureQuality Quality;

    // 编码资源
    TArray<NV_ENC_INPUT_RESOURCE> InputResources;
    TArray<void*> DeviceBuffers;
    TArray<FTextureRHIRef> StagingTextures;

    // D3D设备相关
    void* D3DDevice = nullptr;
    bool bIsInitialized = false;
    bool bIsDeviceCreated = false;
    bool bIsEncoderCreated = false;
};

/**
 * NVENC帧上下文，包含待编码帧的信息
 */
class FNVENCFrameContext
{
public:
    FNVENCFrameContext();
    ~FNVENCFrameContext();

    // GPU帧信息
    TRefCountPtr<IPooledRenderTarget> RenderTarget;
    FGPUFenceRHIRef Fence;

    // CPU帧信息（用于回退路径）
    TArray<uint8> CPUBuffer;
    FIntPoint CPUResolution;
    EPixelFormat CPUFormat;

    // 通用帧信息
    double Timestamp = 0.0;
    bool bIsKeyFrame = false;
    bool bIsCPUFrame = false;
};