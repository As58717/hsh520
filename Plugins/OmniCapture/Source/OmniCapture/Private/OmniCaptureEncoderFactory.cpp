// Copyright Epic Games, Inc. All Rights Reserved.

#include "OmniCaptureEncoderFactory.h"
#include "OmniCaptureNVENCEncoderDirect.h"
#include "Logging/LogMacros.h"

// 定义日志类别
DECLARE_LOG_CATEGORY_EXTERN(LogOmniCaptureEncoder, Log, All);
DEFINE_LOG_CATEGORY(LogOmniCaptureEncoder);

// 自定义编码器注册表
static TMap<EOmniOutputFormat, FOmniCaptureEncoderFactory::EncoderFactoryFunc> CustomEncoderRegistry;
static FCriticalSection CustomEncoderRegistryMutex;

TSharedPtr<IOmniCaptureEncoder> FOmniCaptureEncoderFactory::CreateEncoder(EOmniOutputFormat InFormat)
{
    UE_LOG(LogOmniCaptureEncoder, Log, TEXT("Creating encoder for format: %d"), (int32)InFormat);

    // 首先检查是否有自定义编码器注册
    CustomEncoderRegistryMutex.Lock();
    auto CustomEncoderIt = CustomEncoderRegistry.Find(InFormat);
    if (CustomEncoderIt != nullptr && *CustomEncoderIt)
    {
        TSharedPtr<IOmniCaptureEncoder> CustomEncoder = (*CustomEncoderIt)();
        CustomEncoderRegistryMutex.Unlock();
        
        if (CustomEncoder)
        {
            UE_LOG(LogOmniCaptureEncoder, Log, TEXT("Created custom encoder for format: %d"), (int32)InFormat);
            return CustomEncoder;
        }
    }
    CustomEncoderRegistryMutex.Unlock();

    // 根据格式创建内置编码器
    switch (InFormat)
    {
        case EOmniOutputFormat::NVENCHardware:
        {
            // 创建直接NVENC编码器适配器
            return MakeShareable(new FOmniCaptureDirectNVENCAdapter());
        }
        
        case EOmniOutputFormat::ImageSequence:
        {
            // 这里应该创建图像序列编码器
            // 暂时返回nullptr，实际实现需要添加
            UE_LOG(LogOmniCaptureEncoder, Warning, TEXT("Image sequence encoder not implemented yet"));
            break;
        }
        
        default:
            UE_LOG(LogOmniCaptureEncoder, Error, TEXT("Unsupported encoder format: %d"), (int32)InFormat);
            break;
    }

    return nullptr;
}

bool FOmniCaptureEncoderFactory::IsOutputFormatAvailable(EOmniOutputFormat InFormat)
{
    // 检查是否有自定义编码器注册
    CustomEncoderRegistryMutex.Lock();
    bool bHasCustomEncoder = CustomEncoderRegistry.Contains(InFormat);
    CustomEncoderRegistryMutex.Unlock();
    
    if (bHasCustomEncoder)
    {
        return true;
    }

    // 检查内置编码器可用性
    switch (InFormat)
    {
        case EOmniOutputFormat::NVENCHardware:
            return FOmniCaptureNVENCEncoderDirect::IsNVENCAvailable();
        
        case EOmniOutputFormat::ImageSequence:
            return true; // 图像序列总是可用的
        
        default:
            return false;
    }
}

TArray<EOmniOutputFormat> FOmniCaptureEncoderFactory::GetAvailableOutputFormats()
{
    TArray<EOmniOutputFormat> AvailableFormats;

    // 添加自定义编码器格式
    CustomEncoderRegistryMutex.Lock();
    for (const auto& Pair : CustomEncoderRegistry)
    {
        AvailableFormats.Add(Pair.Key);
    }
    CustomEncoderRegistryMutex.Unlock();

    // 添加可用的内置编码器格式
    if (FOmniCaptureNVENCEncoderDirect::IsNVENCAvailable())
    {
        AvailableFormats.AddUnique(EOmniOutputFormat::NVENCHardware);
    }
    
    AvailableFormats.AddUnique(EOmniOutputFormat::ImageSequence);

    return AvailableFormats;
}

FOmniCaptureEncoderFactory::FEncoderInfo FOmniCaptureEncoderFactory::GetEncoderInfo(EOmniOutputFormat InFormat)
{
    FEncoderInfo Info;
    
    switch (InFormat)
    {
        case EOmniOutputFormat::NVENCHardware:
        {
            Info.DisplayName = TEXT("NVENC Hardware Encoding");
            Info.Description = TEXT("NVIDIA hardware-accelerated video encoding");
            Info.bIsHardwareAccelerated = true;
            Info.bSupportsRealtime = true;
            
            // 获取NVENC能力
            auto Capabilities = FOmniCaptureNVENCEncoderDirect::GetNVENCCapabilities();
            if (Capabilities.bSupportsH264)
            {
                Info.SupportedCodecs.Add(EOmniCaptureCodec::H264);
            }
            if (Capabilities.bSupportsHEVC)
            {
                Info.SupportedCodecs.Add(EOmniCaptureCodec::HEVC);
            }
            if (Capabilities.bSupportsNV12)
            {
                Info.SupportedColorFormats.Add(EOmniCaptureColorFormat::NV12);
            }
            if (Capabilities.bSupportsP010)
            {
                Info.SupportedColorFormats.Add(EOmniCaptureColorFormat::P010);
            }
            if (Capabilities.bSupportsBGRA)
            {
                Info.SupportedColorFormats.Add(EOmniCaptureColorFormat::BGRA);
            }
            break;
        }
        
        case EOmniOutputFormat::ImageSequence:
        {
            Info.DisplayName = TEXT("Image Sequence");
            Info.Description = TEXT("Save frames as individual image files");
            Info.bIsHardwareAccelerated = false;
            Info.bSupportsRealtime = false; // 取决于分辨率和性能
            
            // 图像序列支持所有编解码器（作为文件格式）
            Info.SupportedCodecs.Add(EOmniCaptureCodec::H264); // 实际是PNG/JPG等
            Info.SupportedCodecs.Add(EOmniCaptureCodec::HEVC);
            
            // 支持所有颜色格式
            Info.SupportedColorFormats.Add(EOmniCaptureColorFormat::NV12);
            Info.SupportedColorFormats.Add(EOmniCaptureColorFormat::P010);
            Info.SupportedColorFormats.Add(EOmniCaptureColorFormat::BGRA);
            break;
        }
        
        default:
        {
            Info.DisplayName = TEXT("Unknown Encoder");
            Info.Description = TEXT("No information available");
            break;
        }
    }
    
    return Info;
}

bool FOmniCaptureEncoderFactory::GetRecommendedConfiguration(EOmniOutputFormat InFormat, EOmniCaptureCodec& OutCodec, EOmniCaptureColorFormat& OutColorFormat)
{
    switch (InFormat)
    {
        case EOmniOutputFormat::NVENCHardware:
        {
            // 获取NVENC能力
            auto Capabilities = FOmniCaptureNVENCEncoderDirect::GetNVENCCapabilities();
            
            // 推荐HEVC如果支持，否则H264
            if (Capabilities.bSupportsHEVC)
            {
                OutCodec = EOmniCaptureCodec::HEVC;
            }
            else if (Capabilities.bSupportsH264)
            {
                OutCodec = EOmniCaptureCodec::H264;
            }
            else
            {
                return false;
            }
            
            // 推荐P010如果支持，否则NV12
            if (Capabilities.bSupportsP010)
            {
                OutColorFormat = EOmniCaptureColorFormat::P010;
            }
            else if (Capabilities.bSupportsNV12)
            {
                OutColorFormat = EOmniCaptureColorFormat::NV12;
            }
            else if (Capabilities.bSupportsBGRA)
            {
                OutColorFormat = EOmniCaptureColorFormat::BGRA;
            }
            else
            {
                return false;
            }
            
            return true;
        }
        
        case EOmniOutputFormat::ImageSequence:
        {
            // 图像序列的推荐配置
            OutCodec = EOmniCaptureCodec::H264; // 实际上是PNG
            OutColorFormat = EOmniCaptureColorFormat::BGRA;
            return true;
        }
        
        default:
            return false;
    }
}

void FOmniCaptureEncoderFactory::RegisterCustomEncoder(EOmniOutputFormat InFormat, EncoderFactoryFunc InFactoryFunc)
{
    CustomEncoderRegistryMutex.Lock();
    CustomEncoderRegistry.Add(InFormat, InFactoryFunc);
    CustomEncoderRegistryMutex.Unlock();
    
    UE_LOG(LogOmniCaptureEncoder, Log, TEXT("Registered custom encoder for format: %d"), (int32)InFormat);
}

void FOmniCaptureEncoderFactory::UnregisterCustomEncoder(EOmniOutputFormat InFormat)
{
    CustomEncoderRegistryMutex.Lock();
    CustomEncoderRegistry.Remove(InFormat);
    CustomEncoderRegistryMutex.Unlock();
    
    UE_LOG(LogOmniCaptureEncoder, Log, TEXT("Unregistered custom encoder for format: %d"), (int32)InFormat);
}

// FOmniCaptureDirectNVENCAdapter实现
FOmniCaptureDirectNVENCAdapter::FOmniCaptureDirectNVENCAdapter()
{
    Encoder = MakeShareable(new FOmniCaptureNVENCEncoderDirect());
}

FOmniCaptureDirectNVENCAdapter::~FOmniCaptureDirectNVENCAdapter()
{
    Shutdown();
}

bool FOmniCaptureDirectNVENCAdapter::Initialize(const FIntPoint& InResolution, EOmniCaptureCodec InCodec, const FOmniCaptureQuality& InQuality, EOmniCaptureColorFormat InColorFormat)
{
    if (!Encoder)
    {
        return false;
    }
    
    return Encoder->Initialize(InResolution, InCodec, InQuality, InColorFormat);
}

void FOmniCaptureDirectNVENCAdapter::Shutdown()
{
    if (Encoder)
    {
        Encoder->Shutdown();
    }
}

bool FOmniCaptureDirectNVENCAdapter::EnqueueFrame(const TRefCountPtr<IPooledRenderTarget>& InRenderTarget, const FGPUFenceRHIRef& InFence, double InTimestamp, bool bIsKeyFrame)
{
    if (!Encoder)
    {
        return false;
    }
    
    return Encoder->EnqueueFrame(InRenderTarget, InFence, InTimestamp, bIsKeyFrame);
}

bool FOmniCaptureDirectNVENCAdapter::EnqueueCPUBuffer(const void* InBuffer, const FIntPoint& InResolution, EPixelFormat InFormat, double InTimestamp, bool bIsKeyFrame)
{
    if (!Encoder)
    {
        return false;
    }
    
    return Encoder->EnqueueCPUBuffer(InBuffer, InResolution, InFormat, InTimestamp, bIsKeyFrame);
}

bool FOmniCaptureDirectNVENCAdapter::ProcessEncodedFrames(TFunctionRef<void(const uint8*, uint32, double, bool)> OnFrameEncoded)
{
    if (!Encoder)
    {
        return false;
    }
    
    return Encoder->ProcessEncodedFrames(OnFrameEncoded);
}

void FOmniCaptureDirectNVENCAdapter::Finalize(TFunctionRef<void(const uint8*, uint32, double, bool)> OnFrameEncoded)
{
    if (Encoder)
    {
        Encoder->Finalize(OnFrameEncoded);
    }
}

bool FOmniCaptureDirectNVENCAdapter::IsInitialized() const
{
    if (!Encoder)
    {
        return false;
    }
    
    return Encoder->IsInitialized();
}

EOmniOutputFormat FOmniCaptureDirectNVENCAdapter::GetEncoderType() const
{
    return EOmniOutputFormat::NVENCHardware;
}