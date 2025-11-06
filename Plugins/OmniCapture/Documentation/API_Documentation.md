# OmniCapture 插件 API 文档

本文档详细介绍 OmniCapture 插件的 API 结构、类关系和使用方法，帮助开发者集成和扩展插件功能。

## 1. 核心接口

### 1.1 IOmniCaptureEncoder

编码器接口，定义了所有编码器实现必须提供的方法。

```cpp
class IOmniCaptureEncoder
{
public:
    virtual ~IOmniCaptureEncoder() = default;
    
    // 初始化编码器
    virtual bool Initialize(const FOmniCaptureEncoderConfig& Config) = 0;
    
    // 关闭编码器
    virtual void Shutdown() = 0;
    
    // 检查编码器是否已初始化
    virtual bool IsInitialized() const = 0;
    
    // 入队一帧进行编码
    virtual bool EnqueueFrame(const TRefCountPtr<IPooledRenderTarget>& RenderTarget, 
                             const FGPUFenceRHIRef& Fence, 
                             double Timestamp, 
                             bool bIsKeyFrame = false) = 0;
    
    // 处理已编码的帧
    virtual void ProcessEncodedFrames(const FOnEncodedFrameCallback& Callback) = 0;
    
    // 最终化编码过程
    virtual void Finalize(const FOnEncodedFrameCallback& Callback) = 0;
    
    // 获取编码器类型
    virtual EOmniOutputFormat GetEncoderType() const = 0;
    
    // 获取编码器能力
    virtual const FOmniEncoderCapabilities& GetCapabilities() const = 0;
};
```

**使用示例**：

```cpp
// 创建编码器
TSharedPtr<IOmniCaptureEncoder> Encoder = FOmniCaptureEncoderFactory::CreateEncoder(EOmniOutputFormat::NVENCHardware);

// 配置编码器
FOmniCaptureEncoderConfig Config;
Config.Codec = EOmniCaptureCodec::H264;
Config.Bitrate = 10000000; // 10 Mbps

// 初始化编码器
if (Encoder->Initialize(Config))
{
    // 使用编码器
    // ...
}
```

## 2. 核心类

### 2.1 FOmniCaptureEncoderFactory

编码器工厂类，负责创建和管理编码器实例。

```cpp
class FOmniCaptureEncoderFactory
{
public:
    // 创建编码器
    static TSharedPtr<IOmniCaptureEncoder> CreateEncoder(EOmniOutputFormat Format);
    
    // 检查输出格式是否可用
    static bool IsOutputFormatAvailable(EOmniOutputFormat Format);
    
    // 获取可用的输出格式列表
    static TArray<EOmniOutputFormat> GetAvailableOutputFormats();
    
    // 获取编码器信息
    static FString GetEncoderInfo(EOmniOutputFormat Format);
    
    // 获取推荐的编码器配置
    static FOmniCaptureEncoderConfig GetRecommendedConfiguration(EOmniOutputFormat Format);
    
    // 注册自定义编码器工厂函数
    static void RegisterCustomEncoderFactory(EOmniOutputFormat Format, const FCreateEncoderFactoryFunc& FactoryFunc);
    
    // 注销自定义编码器工厂函数
    static void UnregisterCustomEncoderFactory(EOmniOutputFormat Format);
};
```

**使用示例**：

```cpp
// 检查 NVENC 硬件编码器是否可用
if (FOmniCaptureEncoderFactory::IsOutputFormatAvailable(EOmniOutputFormat::NVENCHardware))
{
    // 创建 NVENC 编码器
    TSharedPtr<IOmniCaptureEncoder> Encoder = 
        FOmniCaptureEncoderFactory::CreateEncoder(EOmniOutputFormat::NVENCHardware);
    
    // 获取推荐配置
    FOmniCaptureEncoderConfig RecommendedConfig = 
        FOmniCaptureEncoderFactory::GetRecommendedConfiguration(EOmniOutputFormat::NVENCHardware);
    
    // 初始化编码器
    Encoder->Initialize(RecommendedConfig);
}
```

### 2.2 FOmniCaptureRenderer

渲染管线集成器，负责捕获渲染帧并传递给编码器。

```cpp
class FOmniCaptureRenderer
{
public:
    FOmniCaptureRenderer();
    virtual ~FOmniCaptureRenderer();
    
    // 初始化渲染器
    bool Initialize(const FOmniCaptureRendererConfig& Config, TSharedPtr<IOmniCaptureEncoder> Encoder);
    
    // 关闭渲染器
    void Shutdown();
    
    // 开始捕获
    void StartCapture();
    
    // 停止捕获
    void StopCapture();
    
    // 暂停捕获
    void PauseCapture();
    
    // 恢复捕获
    void ResumeCapture();
    
    // 更新配置
    void UpdateConfig(const FOmniCaptureRendererConfig& Config);
    
    // 检查是否正在捕获
    bool IsCapturing() const;
    
    // 检查是否暂停
    bool IsPaused() const;
    
    // 获取配置
    const FOmniCaptureRendererConfig& GetConfig() const;
    
    // 设置编码器
    void SetEncoder(TSharedPtr<IOmniCaptureEncoder> Encoder);
    
    // 获取编码器
    TSharedPtr<IOmniCaptureEncoder> GetEncoder() const;
    
protected:
    // 渲染线程帧处理
    void ProcessFrame_RenderThread(FRHICommandListImmediate& RHICmdList, const FSceneView& View);
    
    // 注册渲染事件处理程序
    void RegisterRenderEventHandlers();
    
    // 注销渲染事件处理程序
    void UnregisterRenderEventHandlers();
    
    // 创建渲染目标
    void CreateRenderTargets();
    
    // 释放渲染目标
    void ReleaseRenderTargets();
    
    // 捕获帧到渲染目标
    void CaptureFrameToRenderTarget(FRHICommandListImmediate& RHICmdList, const FSceneView& View);
    
    // 处理捕获的帧
    void ProcessCapturedFrame(const FOmniCaptureRenderFrame& Frame);
    
    // 检查是否应该捕获当前帧
    bool ShouldCaptureCurrentFrame() const;
    
    // 更新帧计时
    void UpdateFrameTiming();
    
    // 创建GPU栅栏
    FGPUFenceRHIRef CreateGPUFence();
    
    // 等待GPU栅栏
    void WaitForGPUFence(const FGPUFenceRHIRef& Fence);
    
    // 转换渲染目标格式
    void ConvertRenderTargetFormat(FRHICommandListImmediate& RHICmdList, 
                                 const TRefCountPtr<IPooledRenderTarget>& SourceRT, 
                                 TRefCountPtr<IPooledRenderTarget>& DestRT);
    
    // 处理编码后的帧
    void HandleEncodedFrame(const uint8* Data, uint32 Size, double Timestamp, bool bIsKeyFrame);
    
    // 处理捕获的帧队列
    void ProcessCapturedFrameQueue();
    
    // 渲染事件处理函数
    void OnPostResolvedSceneColor(FRHICommandListImmediate& RHICmdList, 
                                FSceneRenderTargets& SceneContext, 
                                const FSceneView& View);
};
```

**使用示例**：

```cpp
// 创建渲染器配置
FOmniCaptureRendererConfig RendererConfig;
RendererConfig.Resolution = FIntPoint(1920, 1080);
RendererConfig.CaptureFrequency = 60.0f;

// 创建编码器
TSharedPtr<IOmniCaptureEncoder> Encoder = FOmniCaptureEncoderFactory::CreateEncoder(EOmniOutputFormat::NVENCHardware);

// 创建渲染器
TSharedPtr<FOmniCaptureRenderer> Renderer = MakeShareable(new FOmniCaptureRenderer());

// 初始化渲染器
Renderer->Initialize(RendererConfig, Encoder);

// 开始捕获
Renderer->StartCapture();

// ... 游戏运行 ...

// 停止捕获
Renderer->StopCapture();

// 清理
Renderer->Shutdown();
```

## 3. 配置类

### 3.1 UOmniCaptureNVENCConfig

NVENC 编码器配置类，提供编码参数设置和质量预设。

```cpp
UCLASS(Blueprintable, BlueprintType, Config = Game)
class OMNICAPTURE_API UOmniCaptureNVENCConfig : public UObject
{
    GENERATED_BODY()
    
public:
    // 构造函数
    UOmniCaptureNVENCConfig(const FObjectInitializer& ObjectInitializer);
    
    // 获取默认配置
    static UOmniCaptureNVENCConfig* GetDefault();
    
    // 生成捕获质量配置
    FOmniCaptureEncoderConfig GenerateCaptureQuality() const;
    
    // 验证配置是否有效
    bool IsValidConfig() const;
    
    // 根据硬件能力验证并调整配置
    void ValidateAndAdjustForCapabilities(const FOmniEncoderCapabilities& Capabilities);
    
    // 应用质量预设
    void ApplyQualityPreset(EOmniCaptureQualityPreset Preset);
    
    // 基本编码设置
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic Encoding Settings")
    EOmniCaptureCodec Codec;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic Encoding Settings")
    int32 TargetBitrate; // 比特率，单位：bps
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic Encoding Settings")
    int32 MaxBitrate; // 最大比特率，单位：bps
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic Encoding Settings")
    int32 GOPLength; // 关键帧间隔
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic Encoding Settings")
    int32 MaxBFrames; // 最大B帧数量
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basic Encoding Settings")
    EOmniCaptureProfileLevel ProfileLevel; // 编码规格等级
    
    // 高级编码设置
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced Encoding Settings")
    bool bEnableCUDAAcceleration; // 启用CUDA加速
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced Encoding Settings")
    bool bEnableDynamicGOP; // 启用动态GOP
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced Encoding Settings")
    bool bEnableAdaptiveQuantization; // 启用自适应量化
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced Encoding Settings")
    float QPConstant; // 恒定QP值（如果使用QP控制）
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced Encoding Settings")
    EOmniCaptureRateControl RateControlMode; // 码率控制模式
    
    // 色彩设置
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Color Settings")
    EOmniCaptureColorFormat ColorFormat; // 色彩格式
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Color Settings")
    bool bEnableHDR; // 启用HDR支持
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Color Settings", meta = (EditCondition = "bEnableHDR"))
    float HDRContentLightLevel; // HDR内容亮度级别
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Color Settings", meta = (EditCondition = "bEnableHDR"))
    float HDRMaxContentLightLevel; // HDR最大内容亮度级别
    
    // 诊断选项
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Diagnostics")
    bool bEnableDebugLogging; // 启用调试日志
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Diagnostics")
    bool bEnableStatCollection; // 启用统计收集
};
```

**使用示例**：

```cpp
// 创建配置对象
UOmniCaptureNVENCConfig* Config = NewObject<UOmniCaptureNVENCConfig>();

// 应用预设
Config->ApplyQualityPreset(EOmniCaptureQualityPreset::High);

// 自定义参数
Config->Codec = EOmniCaptureCodec::H265;
Config->TargetBitrate = 15000000; // 15 Mbps
Config->bEnableHDR = true;

// 验证配置
if (Config->IsValidConfig())
{
    // 获取编码器能力
    auto Capabilities = FOmniCaptureNVENCEncoderDirect::GetNVENCCapabilities();
    
    // 根据能力调整配置
    Config->ValidateAndAdjustForCapabilities(Capabilities);
    
    // 生成编码器配置
    FOmniCaptureEncoderConfig EncoderConfig = Config->GenerateCaptureQuality();
}
```

## 4. NVENC 编码器类

### 4.1 FOmniCaptureNVENCEncoderDirect

直接 NVENC 编码器实现，封装了 NVENC API 的使用。

```cpp
class FOmniCaptureNVENCEncoderDirect : public IOmniCaptureEncoder
{
public:
    // 构造函数和析构函数
    FOmniCaptureNVENCEncoderDirect();
    virtual ~FOmniCaptureNVENCEncoderDirect();
    
    // 从 IOmniCaptureEncoder 接口实现的方法
    virtual bool Initialize(const FOmniCaptureEncoderConfig& Config) override;
    virtual void Shutdown() override;
    virtual bool IsInitialized() const override;
    virtual bool EnqueueFrame(const TRefCountPtr<IPooledRenderTarget>& RenderTarget, 
                             const FGPUFenceRHIRef& Fence, 
                             double Timestamp, 
                             bool bIsKeyFrame = false) override;
    virtual void ProcessEncodedFrames(const FOnEncodedFrameCallback& Callback) override;
    virtual void Finalize(const FOnEncodedFrameCallback& Callback) override;
    virtual EOmniOutputFormat GetEncoderType() const override;
    virtual const FOmniEncoderCapabilities& GetCapabilities() const override;
    
    // 静态方法
    static bool IsNVENCAvailable();
    static FOmniEncoderCapabilities GetNVENCCapabilities();
    
protected:
    // 编码线程类
    class FNVENCEncodeThread : public FRunnable
    {
    public:
        FNVENCEncodeThread(FOmniCaptureNVENCEncoderDirect* InEncoder);
        virtual ~FNVENCEncodeThread();
        
        virtual bool Init() override;
        virtual uint32 Run() override;
        virtual void Stop() override;
        virtual void Exit() override;
        
        void EnqueueFrame(const FNVENCFrameContext& FrameContext);
        void ProcessEncodedFrames(const FOnEncodedFrameCallback& Callback);
        
    private:
        FOmniCaptureNVENCEncoderDirect* Encoder;
        FRunnableThread* Thread;
        FEvent* StopEvent;
        FEvent* FrameAvailableEvent;
        TQueue<FNVENCFrameContext, EQueueMode::Mpsc> FrameQueue;
        // ... 其他成员
    };
    
private:
    // NVENC 会话
    TUniquePtr<FNVENCEncoderSession> EncoderSession;
    
    // 编码线程
    TUniquePtr<FNVENCEncodeThread> EncodeThread;
    
    // 编码器配置
    FOmniCaptureEncoderConfig Config;
    
    // 编码器能力
    FOmniEncoderCapabilities Capabilities;
    
    // 初始化状态
    bool bIsInitialized;
    
    // 内部方法
    bool InitializeNVENC();
    bool CreateEncoderSession();
    bool PrepareResources();
    void ReleaseResources();
    // ... 其他内部方法
};
```

### 4.2 FNVENCEncoderSession

NVENC 编码会话类，封装了 NVENC API 的核心功能。

```cpp
class FNVENCEncoderSession
{
public:
    FNVENCEncoderSession();
    ~FNVENCEncoderSession();
    
    // 初始化会话
    bool Initialize(const FOmniCaptureEncoderConfig& Config);
    
    // 关闭会话
    void Shutdown();
    
    // 检查会话是否已初始化
    bool IsInitialized() const;
    
    // 创建设备
    bool CreateDevice();
    
    // 创建编码器
    bool CreateEncoder();
    
    // 分配编码资源
    bool AllocateResources();
    
    // 释放编码资源
    void ReleaseResources();
    
    // 编码纹理
    bool EncodeTexture(const FTextureRHIRef& Texture, double Timestamp, bool bIsKeyFrame);
    
    // 编码缓冲区
    bool EncodeBuffer(const void* Buffer, uint32 Width, uint32 Height, uint32 Pitch, double Timestamp, bool bIsKeyFrame);
    
    // 处理编码输出
    bool ProcessOutput(const FOnEncodedFrameCallback& Callback);
    
    // 获取编码器能力
    FOmniEncoderCapabilities GetCapabilities() const;
    
    // 检查配置是否支持
    bool IsConfigSupported(const FOmniCaptureEncoderConfig& Config) const;
    
private:
    // NVENC API 函数列表
    NV_ENCODE_API_FUNCTION_LIST NvEncodeAPIFuncList;
    
    // NVENC 资源
    void* NvEncoderInterface;
    void* NvEncoderGUID;
    
    // 设备和会话
    void* NvEncoderDevice;
    void* NvEncoderSession;
    
    // 编码配置
    FOmniCaptureEncoderConfig Config;
    
    // 编码资源
    TArray<void*> InputSurfaces;
    TArray<void*> OutputBitstreams;
    
    // 设备相关
    ID3D11Device* D3DDevice;
    ID3D11DeviceContext* D3DDeviceContext;
    
    // 初始化状态
    bool bIsInitialized;
    bool bIsNvEncodeAPILoaded;
    
    // 内部方法
    bool LoadNVEncodeAPI();
    bool InitializeNVEncodeAPIFunctions();
    void UnloadNVEncodeAPI();
    
    // 检查 NVENC 可用性
    bool CheckNVENCAvailability();
    
    // 获取编码器能力
    bool QueryEncoderCapabilities();
    
    // 创建编码输入表面
    bool CreateInputSurfaces();
    
    // 创建编码输出流
    bool CreateOutputBitstreams();
    
    // 提交帧到编码器
    bool SubmitFrame(const void* FrameData, uint32 Width, uint32 Height, uint32 Pitch, bool bIsKeyFrame);
    
    // 从编码器获取输出
    bool GetEncodedOutput(const FOnEncodedFrameCallback& Callback);
};
```

## 5. 组件类

### 5.1 UOmniCaptureRenderComponent

渲染捕获组件，可添加到任何 Actor 上用于视频捕获。

```cpp
UCLASS(Blueprintable, ClassGroup = (OmniCapture), meta = (BlueprintSpawnableComponent))
class OMNICAPTURE_API UOmniCaptureRenderComponent : public UActorComponent
{
    GENERATED_BODY()
    
public:
    // 构造函数
    UOmniCaptureRenderComponent(const FObjectInitializer& ObjectInitializer);
    
    // 从 UActorComponent 继承的方法
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnRegister() override;
    virtual void OnUnregister() override;
    
    // 开始捕获
    UFUNCTION(BlueprintCallable, Category = "OmniCapture")
    void StartCapture();
    
    // 停止捕获
    UFUNCTION(BlueprintCallable, Category = "OmniCapture")
    void StopCapture();
    
    // 暂停捕获
    UFUNCTION(BlueprintCallable, Category = "OmniCapture")
    void PauseCapture();
    
    // 恢复捕获
    UFUNCTION(BlueprintCallable, Category = "OmniCapture")
    void ResumeCapture();
    
    // 设置分辨率
    UFUNCTION(BlueprintCallable, Category = "OmniCapture")
    void SetResolution(FIntPoint NewResolution);
    
    // 设置捕获帧率
    UFUNCTION(BlueprintCallable, Category = "OmniCapture")
    void SetCaptureFrameRate(float NewFrameRate);
    
    // 设置HDR捕获
    UFUNCTION(BlueprintCallable, Category = "OmniCapture")
    void SetCaptureHDR(bool NewCaptureHDR);
    
    // 设置输出格式
    UFUNCTION(BlueprintCallable, Category = "OmniCapture")
    void SetOutputFormat(EOmniOutputFormat NewFormat);
    
    // 检查是否正在捕获
    UFUNCTION(BlueprintCallable, Category = "OmniCapture")
    bool IsCapturing() const;
    
    // 检查是否暂停
    UFUNCTION(BlueprintCallable, Category = "OmniCapture")
    bool IsPaused() const;
    
    // 获取当前配置
    UFUNCTION(BlueprintCallable, Category = "OmniCapture")
    FOmniCaptureRendererConfig GetCurrentConfig() const;
    
    // 蓝图可实现事件
    UFUNCTION(BlueprintImplementableEvent, Category = "OmniCapture")
    void ReceiveCaptureStarted();
    
    UFUNCTION(BlueprintImplementableEvent, Category = "OmniCapture")
    void ReceiveCaptureStopped();
    
    UFUNCTION(BlueprintImplementableEvent, Category = "OmniCapture")
    void ReceiveCaptureError(const FString& ErrorMessage);
    
    // 公共属性
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture Settings")
    FIntPoint CaptureResolution;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture Settings")
    bool bCaptureHDR;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture Settings")
    bool bCaptureAlpha;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture Settings")
    float CaptureFrameRate;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture Settings")
    bool bLimitFrameRate;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture Settings")
    EOmniOutputFormat OutputFormat;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capture Settings")
    bool bAutoStartCapture;
    
protected:
    // 渲染器指针
    TSharedPtr<FOmniCaptureRenderer> Renderer;
    
    // 编码器指针
    TSharedPtr<IOmniCaptureEncoder> Encoder;
    
    // 初始化渲染器
    void InitializeRenderer();
    
    // 创建编码器
    TSharedPtr<IOmniCaptureEncoder> CreateEncoder();
    
    // 处理错误
    void HandleError(const FString& ErrorMessage);
};
```

**使用示例**：

```cpp
// 在Actor中使用捕获组件
void AMyActor::BeginPlay()
{
    Super::BeginPlay();
    
    // 创建并初始化捕获组件
    CaptureComponent = NewObject<UOmniCaptureRenderComponent>(this);
    CaptureComponent->RegisterComponent();
    
    // 配置捕获组件
    CaptureComponent->CaptureResolution = FIntPoint(1920, 1080);
    CaptureComponent->CaptureFrameRate = 60.0f;
    CaptureComponent->bCaptureHDR = false;
    
    // 开始捕获
    CaptureComponent->StartCapture();
}

void AMyActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 停止捕获
    if (CaptureComponent)
    {
        CaptureComponent->StopCapture();
        CaptureComponent->UnregisterComponent();
    }
    
    Super::EndPlay(EndPlayReason);
}
```

## 6. 数据结构

### 6.1 FOmniCaptureRendererConfig

渲染捕获配置结构。

```cpp
struct FOmniCaptureRendererConfig
{
    // 捕获分辨率
    FIntPoint Resolution = FIntPoint(1920, 1080);
    
    // 是否捕获HDR内容
    bool bCaptureHDR = false;
    
    // 是否捕获Alpha通道
    bool bCaptureAlpha = false;
    
    // 捕获帧率
    float CaptureFrequency = 60.0f;
    
    // 是否限制帧率
    bool bLimitFrameRate = true;
};
```

### 6.2 FOmniCaptureEncoderConfig

编码器配置结构。

```cpp
struct FOmniCaptureEncoderConfig
{
    // 编码器类型
    EOmniOutputFormat OutputFormat = EOmniOutputFormat::NVENCHardware;
    
    // 编解码器
    EOmniCaptureCodec Codec = EOmniCaptureCodec::H264;
    
    // 目标比特率（bps）
    int32 TargetBitrate = 10000000; // 10 Mbps
    
    // 最大比特率（bps）
    int32 MaxBitrate = 15000000; // 15 Mbps
    
    // GOP长度
    int32 GOPLength = 30; // 一个关键帧
    
    // 最大B帧数量
    int32 MaxBFrames = 2;
    
    // 编码规格等级
    EOmniCaptureProfileLevel ProfileLevel = EOmniCaptureProfileLevel::High;
    
    // 色彩格式
    EOmniCaptureColorFormat ColorFormat = EOmniCaptureColorFormat::NV12;
    
    // 码率控制模式
    EOmniCaptureRateControl RateControlMode = EOmniCaptureRateControl::CBR;
    
    // 是否启用HDR
    bool bEnableHDR = false;
    
    // 是否启用CUDA加速
    bool bEnableCUDAAcceleration = true;
};
```

### 6.3 FOmniEncoderCapabilities

编码器能力结构。

```cpp
struct FOmniEncoderCapabilities
{
    // 是否支持H.264
    bool bSupportsH264 = false;
    
    // 是否支持H.265/HEVC
    bool bSupportsH265 = false;
    
    // 是否支持NV12色彩格式
    bool bSupportsNV12 = false;
    
    // 是否支持P010色彩格式（用于HDR）
    bool bSupportsP010 = false;
    
    // 是否支持B帧
    bool bSupportsBFrames = false;
    
    // 是否支持动态GOP
    bool bSupportsDynamicGOP = false;
    
    // 是否支持自适应量化
    bool bSupportsAdaptiveQuantization = false;
    
    // 最大支持的分辨率
    FIntPoint MaxResolution = FIntPoint(0, 0);
    
    // 支持的帧率范围
    FFloatRange SupportedFrameRateRange = FFloatRange(1.0f, 240.0f);
    
    // 支持的比特率范围
    FInt32Range SupportedBitrateRange = FInt32Range(100000, 100000000); // 100 Kbps to 100 Mbps
};
```

## 7. 枚举类型

### 7.1 EOmniOutputFormat

输出格式枚举。

```cpp
enum class EOmniOutputFormat
{
    Invalid = 0,
    NVENCHardware = 1, // NVENC硬件编码器
    Software = 2,      // 软件编码器（预留）
    External = 3       // 外部编码器（预留）
};
```

### 7.2 EOmniCaptureCodec

编解码器枚举。

```cpp
enum class EOmniCaptureCodec
{
    Invalid = 0,
    H264 = 1,  // H.264/AVC
    H265 = 2   // H.265/HEVC
};
```

### 7.3 EOmniCaptureProfileLevel

编码规格等级枚举。

```cpp
enum class EOmniCaptureProfileLevel
{
    Baseline = 0,
    Main = 1,
    High = 2,
    High10 = 3,
    High422 = 4,
    High444 = 5
};
```

### 7.4 EOmniCaptureColorFormat

色彩格式枚举。

```cpp
enum class EOmniCaptureColorFormat
{
    NV12 = 0,  // YUV 4:2:0 8-bit
    P010 = 1,  // YUV 4:2:0 10-bit (用于HDR)
    YUY2 = 2,  // YUV 4:2:2 8-bit
    RGBA = 3   // RGBA 8-bit
};
```

### 7.5 EOmniCaptureRateControl

码率控制模式枚举。

```cpp
enum class EOmniCaptureRateControl
{
    CBR = 0,    // 恒定比特率
    VBR = 1,    // 可变比特率
    CQP = 2,    // 恒定QP
    VBRHq = 3   // 高质量可变比特率
};
```

### 7.6 EOmniCaptureQualityPreset

质量预设枚举。

```cpp
enum class EOmniCaptureQualityPreset
{
    Low = 0,      // 低质量
    Balanced = 1, // 平衡（默认）
    High = 2,     // 高质量
    Ultra = 3,    // 超高质量
    Lossless = 4  // 无损
};
```

## 8. 回调函数

### 8.1 FOnEncodedFrameCallback

编码帧回调函数类型。

```cpp
DECLARE_DELEGATE_FourParams(FOnEncodedFrameCallback, const uint8* /* Data */, uint32 /* Size */, double /* Timestamp */, bool /* bIsKeyFrame */);
```

**使用示例**：

```cpp
// 定义回调函数
FOnEncodedFrameCallback OnEncodedFrame = FOnEncodedFrameCallback::CreateLambda(
    [this](const uint8* Data, uint32 Size, double Timestamp, bool bIsKeyFrame) {
        // 处理编码后的帧数据
        if (Data && Size > 0)
        {
            // 写入文件或通过网络发送
            WriteFrameToFile(Data, Size);
            
            // 记录关键帧信息
            if (bIsKeyFrame)
            {
                UE_LOG(LogMyGame, Log, TEXT("Key frame received at %.3f"), Timestamp);
            }
        }
    });

// 设置编码器回调
Encoder->ProcessEncodedFrames(OnEncodedFrame);
```

## 9. 常见问题解答

### 9.1 如何检查NVENC是否可用？

```cpp
// 使用静态方法检查
if (FOmniCaptureNVENCEncoderDirect::IsNVENCAvailable())
{
    UE_LOG(LogMyGame, Log, TEXT("NVENC is available"));
    
    // 获取详细能力
    auto Capabilities = FOmniCaptureNVENCEncoderDirect::GetNVENCCapabilities();
    UE_LOG(LogMyGame, Log, TEXT("H.265 support: %d, Max resolution: %dx%d"),
        Capabilities.bSupportsH265 ? 1 : 0,
        Capabilities.MaxResolution.X,
        Capabilities.MaxResolution.Y);
}
else
{
    UE_LOG(LogMyGame, Error, TEXT("NVENC is not available on this system"));
}
```

### 9.2 如何处理不同分辨率和帧率的捕获？

```cpp
// 创建渲染器配置
FOmniCaptureRendererConfig RendererConfig;

// 设置不同分辨率
RendererConfig.Resolution = FIntPoint(3840, 2160); // 4K

// 设置帧率
RendererConfig.CaptureFrequency = 30.0f;

// 禁用帧率限制（捕获所有帧）
RendererConfig.bLimitFrameRate = false;

// 初始化渲染器
Renderer->Initialize(RendererConfig, Encoder);
```

### 9.3 如何实现HDR内容捕获？

```cpp
// 确保使用支持HDR的配置
FOmniCaptureRendererConfig RendererConfig;
RendererConfig.bCaptureHDR = true;

// 配置编码器支持HDR
FOmniCaptureEncoderConfig EncoderConfig;
EncoderConfig.bEnableHDR = true;
EncoderConfig.ColorFormat = EOmniCaptureColorFormat::P010; // 使用10位色彩格式

// 检查编码器是否支持HDR
if (Capabilities.bSupportsP010)
{
    // 初始化编码器和渲染器
    Encoder->Initialize(EncoderConfig);
    Renderer->Initialize(RendererConfig, Encoder);
}
```

### 9.4 如何优化编码性能？

```cpp
// 选择较低的质量预设
UOmniCaptureNVENCConfig* Config = NewObject<UOmniCaptureNVENCConfig>();
Config->ApplyQualityPreset(EOmniCaptureQualityPreset::Low);

// 降低分辨率
Config->Resolution = FIntPoint(1280, 720);

// 降低帧率
Config->CaptureFrameRate = 30.0f;

// 使用H.264而非H.265（对于较老的GPU可能更快）
Config->Codec = EOmniCaptureCodec::H264;

// 减少B帧数量
Config->MaxBFrames = 0;

// 使用CBR码率控制
Config->RateControlMode = EOmniCaptureRateControl::CBR;
```

## 10. 性能监控与调试

### 10.1 启用调试日志

```cpp
// 在配置中启用调试日志
Config->bEnableDebugLogging = true;

// 查看日志输出
// 日志会输出到UE编辑器的输出日志窗口
// 以及项目的日志文件中
```

### 10.2 监控编码统计信息

```cpp
// 定义统计信息变量
float AverageBitrate = 0.0f;
float CurrentFPS = 0.0f;

// 在Tick中更新统计信息
void AMyActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    if (CaptureComponent && CaptureComponent->IsCapturing())
    {
        // 从测试Actor获取统计信息
        TestActor->GetEncodingStatistics(AverageBitrate, CurrentFPS);
        
        // 显示或记录统计信息
        GEngine->AddOnScreenDebugMessage(-1, 0.1f, FColor::Green,
            FString::Printf(TEXT("Bitrate: %.2f Mbps | FPS: %.1f"), AverageBitrate, CurrentFPS));
    }
}
```

## 11. 扩展插件

### 11.1 注册自定义编码器

```cpp
// 定义自定义编码器工厂函数
auto CustomEncoderFactory = [](EOmniOutputFormat Format) -> TSharedPtr<IOmniCaptureEncoder> {
    if (Format == EOmniOutputFormat::Custom)
    {
        return MakeShareable(new FCustomEncoder());
    }
    return nullptr;
};

// 注册自定义编码器
FOmniCaptureEncoderFactory::RegisterCustomEncoderFactory(
    EOmniOutputFormat::Custom, 
    CustomEncoderFactory);

// 使用自定义编码器
TSharedPtr<IOmniCaptureEncoder> CustomEncoder = 
    FOmniCaptureEncoderFactory::CreateEncoder(EOmniOutputFormat::Custom);
```

### 11.2 自定义渲染捕获流程

```cpp
class FCustomCaptureRenderer : public FOmniCaptureRenderer
{
protected:
    // 重写捕获帧方法
    virtual void CaptureFrameToRenderTarget(FRHICommandListImmediate& RHICmdList, const FSceneView& View) override
    {
        // 自定义帧捕获逻辑
        // 例如：应用自定义后处理效果
        ApplyCustomPostProcessing(RHICmdList, View);
        
        // 调用父类方法完成剩余处理
        Super::CaptureFrameToRenderTarget(RHICmdList, View);
    }
    
private:
    // 应用自定义后处理
    void ApplyCustomPostProcessing(FRHICommandListImmediate& RHICmdList, const FSceneView& View)
    {
        // 实现自定义后处理逻辑
        // ...
    }
};

// 使用自定义渲染器
TSharedPtr<FOmniCaptureRenderer> CustomRenderer = MakeShareable(new FCustomCaptureRenderer());
```

---

本文档提供了 OmniCapture 插件的完整 API 参考。开发者可以根据需要集成和扩展插件功能，实现高质量的视频捕获应用。