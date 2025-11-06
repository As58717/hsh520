# OmniCapture NVENC 插件

OmniCapture 是一个为 Unreal Engine 5.6 设计的高性能视频捕获插件，直接集成 NVIDIA NVENC 硬件编码器，无需使用 AVE（Advanced Video Extension）。该插件提供了实时的高质量视频捕获功能，适用于游戏内录制、截图、视频流等场景。

## 功能特点

- **直接 NVENC 集成**：绕过 AVE，直接与 NVIDIA NVENC API 交互
- **高性能硬件编码**：利用 GPU 硬件加速，最小化 CPU 占用
- **支持多种编码格式**：包括 H.264/AVC 和 H.265/HEVC
- **HDR 支持**：支持高动态范围内容捕获
- **自定义编码参数**：可调整比特率、GOP 大小、质量预设等
- **游戏内 UI 控制**：提供测试 UI 用于实时监控和控制捕获过程
- **与 UE5 渲染管线深度集成**：无缝捕获渲染帧

## 系统要求

- Unreal Engine 5.6
- NVIDIA GPU，支持 NVENC 编码（GeForce GTX 900 系列或更高版本，专业卡 Quadro K 系列或更高版本）
- Windows 10/11 64 位操作系统
- 最新的 NVIDIA 驱动程序

## 安装方法

1. 将 `OmniCapture` 文件夹复制到您的 UE5.6 项目的 `Plugins` 目录中
2. 重新启动 Unreal Engine
3. 在编辑器中，导航到 **编辑 > 插件**，确保 "OmniCapture" 插件已启用

## 使用指南

### 通过蓝图使用

1. 在关卡中放置一个 `AOmniCaptureTestActor` 实例
2. 创建一个 `UOmniCaptureTestUI` 小部件并添加到玩家界面
3. 在蓝图中设置 UI 与测试 Actor 的关联

```blueprint
// 示例：设置测试 UI
YourOmniCaptureTestUI->SetTestActor(YourOmniCaptureTestActor);
```

### 通过代码使用

1. 在您的项目中包含必要的头文件

```cpp
#include "OmniCaptureRenderer.h"
#include "OmniCaptureNVENCConfig.h"
```

2. 创建并初始化捕获系统

```cpp
// 创建配置
UOmniCaptureNVENCConfig* Config = NewObject<UOmniCaptureNVENCConfig>();
Config->ApplyQualityPreset(EOmniCaptureQualityPreset::Balanced);

// 创建渲染器配置
FOmniCaptureRendererConfig RendererConfig;
RendererConfig.Resolution = FIntPoint(1920, 1080);
RendererConfig.bCaptureHDR = false;
RendererConfig.CaptureFrequency = 60.0f;

// 创建编码器
TSharedPtr<IOmniCaptureEncoder> Encoder = FOmniCaptureEncoderFactory::CreateEncoder(EOmniOutputFormat::NVENCHardware);

// 创建并初始化渲染器
TSharedPtr<FOmniCaptureRenderer> Renderer = MakeShareable(new FOmniCaptureRenderer());
Renderer->Initialize(RendererConfig, Encoder);

// 开始捕获
Renderer->StartCapture();
```

### 使用组件

您可以在任何 Actor 上添加 `UOmniCaptureRenderComponent` 组件，然后直接通过组件控制捕获：

```cpp
// 获取或创建捕获组件
UOmniCaptureRenderComponent* CaptureComponent = GetComponentByClass<UOmniCaptureRenderComponent>();
if (!CaptureComponent)
{
    CaptureComponent = NewObject<UOmniCaptureRenderComponent>(this);
    CaptureComponent->RegisterComponent();
}

// 配置并开始捕获
CaptureComponent->SetResolution(FIntPoint(1920, 1080));
CaptureComponent->SetCaptureFrameRate(60.0f);
CaptureComponent->StartCapture();
```

## 配置选项

### 编码设置

- **编解码器**：H.264 或 H.265 (HEVC)
- **比特率**：可设置固定比特率或使用可变比特率
- **GOP 大小**：关键帧间隔
- **预设**：从低到无损的多种质量预设

### 质量预设

插件提供了以下预定义的质量预设：

- **低质量**：低比特率，高压缩，适合长时间录制或网络流媒体
- **平衡**：默认设置，平衡质量和文件大小
- **高质量**：较高比特率，适合大多数录制场景
- **超高质量**：高比特率，近无损质量
- **无损**：使用 NVENC 的无损编码模式

### 渲染捕获设置

- **分辨率**：捕获分辨率
- **帧率**：捕获帧率
- **HDR 捕获**：是否启用 HDR 内容捕获
- **Alpha 通道**：是否捕获 Alpha 通道

## 测试工具

插件包含以下测试工具：

### AOmniCaptureTestActor

- 提供完整的捕获功能测试
- 支持不同质量预设、分辨率和帧率测试
- 可设置捕获持续时间
- 提供编码统计信息

### UOmniCaptureTestUI

- 交互式 UI 控件，用于开始/停止/暂停/恢复捕获
- 质量预设选择
- 分辨率、帧率和持续时间设置
- 实时显示捕获状态和编码统计信息
- 进度条显示（当设置了捕获持续时间时）

## 性能优化提示

1. **选择合适的分辨率和帧率**：根据您的需求平衡质量和性能
2. **使用合适的预设**：对于实时应用，考虑使用较低的预设
3. **避免频繁的关键帧**：增加 GOP 大小可以减少文件大小，但可能影响编辑性能
4. **监控编码统计**：使用测试 UI 监控编码性能和比特率

## 已知限制

- 仅支持 NVIDIA GPU
- HDR 捕获在某些显示器和回放设备上可能不兼容
- 高分辨率和高帧率捕获可能受到 GPU 性能限制

## 故障排除

### 编码器不可用

- 确保您的 NVIDIA GPU 支持 NVENC
- 更新到最新的 NVIDIA 驱动程序
- 检查是否有其他应用程序正在使用 NVENC

### 性能问题

- 降低捕获分辨率或帧率
- 使用较低的质量预设
- 关闭不必要的特效和后处理

### 编码错误

- 检查输出目录是否有写入权限
- 确保有足够的磁盘空间
- 尝试降低编码质量或分辨率

## 开发指南

### 扩展编码器

如需添加新的编码器实现，请遵循以下步骤：

1. 实现 `IOmniCaptureEncoder` 接口
2. 在 `FOmniCaptureEncoderFactory` 中注册您的编码器

### 自定义渲染捕获

如需自定义渲染捕获流程，可以继承 `FOmniCaptureRenderer` 类并重写相关方法。

## 许可证

本插件仅供示例和学习使用。商业使用请联系开发团队。

## 联系方式

如有问题或建议，请联系插件开发团队。

## 更新日志

### 1.0.0
- 初始版本发布
- 支持 H.264 和 H.265 编码
- 实现与 UE5.6 渲染管线集成
- 添加测试工具和 UI
