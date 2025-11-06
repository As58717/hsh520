// Copyright Epic Games, Inc. All Rights Reserved.

#include "OmniCaptureNVENCStatus.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SVerticalBox.h"
#include "Widgets/Layout/SHorizontalBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Progress/SProgressBar.h"
#include "Widgets/Input/SFileDialog.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandList.h"
#include "EditorStyleSet.h"
#include "Editor.h"
#include "Internationalization/Internationalization.h"
#include "Misc/MessageDialog.h"
#include "Misc/CoreDelegates.h"
#include "TimerManager.h"

#include "OmniCaptureNVENCAPI.h"

#define LOCTEXT_NAMESPACE "OmniCaptureNVENCStatus"

// 支持的分辨率列表
const TArray<FIntPoint> SupportedResolutionsList = {
    FIntPoint(320, 240),
    FIntPoint(640, 480),
    FIntPoint(854, 480),
    FIntPoint(1280, 720),
    FIntPoint(1920, 1080),
    FIntPoint(2560, 1440),
    FIntPoint(3840, 2160),
    FIntPoint(7680, 4320)
};

// 支持的编码器列表
const TArray<FString> SupportedEncodersList = {
    TEXT("H264"),
    TEXT("H265/HEVC"),
    TEXT("AV1") // 较新的GPU支持
};

void SOmniCaptureNVENCStatus::Construct(const FArguments& InArgs)
{
    bAutoRefresh = InArgs._bAutoRefresh;
    RefreshInterval = InArgs._RefreshInterval;
    OnStatusChanged = InArgs._OnStatusChanged;
    
    // 初始化状态变量
    bNVENCAvailable = false;
    MaxEncoderSessions = 0;
    
    // 构造UI
    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(8.0f)
        [
            SNew(SVerticalBox)
            
            // 状态行
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 4.0f)
            [
                SNew(SHorizontalBox)
                
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.0f, 0.0f, 4.0f, 0.0f)
                [
                    SNew(SImage)
                    .Image(this, &SOmniCaptureNVENCStatus::GetStatusIcon)
                    .ColorAndOpacity(this, &SOmniCaptureNVENCStatus::GetStatusColor)
                    .DesiredSizeScale(FVector2D(1.0f, 1.0f))
                ]
                
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNew(STextBlock)
                    .Text_Lambda([this]() { return FText::FromString(GetStatusText()); })
                    .Font(FEditorStyle::GetFontStyle("BoldFont"))
                    .ColorAndOpacity(this, &SOmniCaptureNVENCStatus::GetStatusColor)
                ]
                
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(4.0f, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("RefreshButton", "Refresh"))
                    .ToolTipText(LOCTEXT("RefreshTooltip", "Refresh NVENC status"))
                    .OnClicked_Lambda([this]() 
                    {
                        RefreshNVENCStatus();
                        return FReply::Handled();
                    })
                ]
            ]
            
            // 详细信息区域
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 4.0f, 0.0f, 4.0f)
            [
                SNew(STextBlock)
                .Text_Lambda([this]() { return FText::FromString(GetDetailedInfo()); })
                .Font(FEditorStyle::GetFontStyle("SmallFont"))
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                .Visibility_Lambda([this]() { return GetDetailedInfo().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
            ]
            
            // 信息网格
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 4.0f, 0.0f, 4.0f)
            [
                SNew(SHorizontalBox)
                
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNew(SVerticalBox)
                    
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SAssignNew(DriverVersionTextBlock, STextBlock)
                        .Text(LOCTEXT("DriverVersionPlaceholder", "Driver Version: Checking..."))
                        .Font(FEditorStyle::GetFontStyle("SmallFont"))
                    ]
                    
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SAssignNew(SessionsTextBlock, STextBlock)
                        .Text(LOCTEXT("SessionsPlaceholder", "Max Encoder Sessions: Checking..."))
                        .Font(FEditorStyle::GetFontStyle("SmallFont"))
                    ]
                ]
                
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNew(SVerticalBox)
                    
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SAssignNew(LibraryPathTextBlock, STextBlock)
                        .Text(LOCTEXT("LibraryPathPlaceholder", "Library Path: Checking..."))
                        .Font(FEditorStyle::GetFontStyle("SmallFont"))
                        .AutoWrapText(true)
                    ]
                ]
            ]
            
            // 操作按钮区域
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 4.0f, 0.0f, 0.0f)
            [
                SNew(SHorizontalBox)
                
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.0f, 0.0f, 4.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("BrowseButton", "Browse NVENC Library"))
                    .ToolTipText(LOCTEXT("BrowseTooltip", "Manually select NVENC library file"))
                    .OnClicked_Lambda([this]() 
                    {
                        OnBrowseNVENCLibrary();
                        return FReply::Handled();
                    })
                ]
                
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.0f, 0.0f, 4.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("TestButton", "Test NVENC"))
                    .ToolTipText(LOCTEXT("TestTooltip", "Run NVENC functionality test"))
                    .OnClicked_Lambda([this]() 
                    {
                        OnTestNVENCFunctionality();
                        return FReply::Handled();
                    })
                ]
                
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .Text(LOCTEXT("ResetButton", "Reset to Default"))
                    .ToolTipText(LOCTEXT("ResetTooltip", "Reset to default NVENC settings"))
                    .OnClicked_Lambda([this]() 
                    {
                        OnResetToDefault();
                        return FReply::Handled();
                    })
                ]
            ]
            
            // 刷新进度条
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 4.0f, 0.0f, 0.0f)
            .Visibility_Lambda([this]() { return bAutoRefresh ? EVisibility::Visible : EVisibility::Collapsed; })
            [
                SNew(SBox)
                .HeightOverride(6.0f)
                [
                    SAssignNew(RefreshProgressBar, SProgressBar)
                    .FillColorAndOpacity(FSlateColor::UseAccentColor())
                    .Percent_Lambda([this]() 
                    {
                        static double LastRefreshTime = FPlatformTime::Seconds();
                        double CurrentTime = FPlatformTime::Seconds();
                        double Elapsed = CurrentTime - LastRefreshTime;
                        if (Elapsed >= RefreshInterval)
                        {
                            LastRefreshTime = CurrentTime;
                        }
                        return FMath::Min(1.0f, (float)(Elapsed / RefreshInterval));
                    })
                ]
            ]
        ]
    ];
    
    // 初始刷新
    RefreshNVENCStatus();
    
    // 启动自动刷新
    if (bAutoRefresh)
    {
        StartAutoRefresh(RefreshInterval);
    }
}

SOmniCaptureNVENCStatus::~SOmniCaptureNVENCStatus()
{
    // 停止自动刷新
    StopAutoRefresh();
}

void SOmniCaptureNVENCStatus::RefreshNVENCStatus()
{
    // 使用NVENC API管理器检查状态
    FOmniCaptureNVENCAPI& NVENCAPI = FOmniCaptureNVENCAPI::Get();
    
    // 尝试加载NVENC API
    if (!NVENCAPI.IsNVEncodeAPILoaded())
    {
        NVENCAPI.LoadNVEncodeAPI();
    }
    
    // 更新状态
    bool bWasAvailable = bNVENCAvailable;
    bNVENCAvailable = NVENCAPI.IsNVENCAvailable();
    DriverVersion = NVENCAPI.GetNVENCDriverVersion();
    LibraryPath = NVENCAPI.GetNVEncodeAPILibraryPath();
    MaxEncoderSessions = NVENCAPI.GetMaxEncoderSessions();
    HardwareInfo = NVENCAPI.GetNVENCHardwareInfo();
    
    // 更新UI
    UpdateUIDisplay();
    
    // 触发状态改变回调
    if (bWasAvailable != bNVENCAvailable && OnStatusChanged.IsBound())
    {
        OnStatusChanged.Execute(bNVENCAvailable);
    }
}

bool SOmniCaptureNVENCStatus::IsNVENCAvailable() const
{
    return bNVENCAvailable;
}

FString SOmniCaptureNVENCStatus::GetDriverVersion() const
{
    return DriverVersion;
}

FString SOmniCaptureNVENCStatus::GetLibraryPath() const
{
    return LibraryPath;
}

int32 SOmniCaptureNVENCStatus::GetMaxEncoderSessions() const
{
    return MaxEncoderSessions;
}

FString SOmniCaptureNVENCStatus::GetHardwareInfo() const
{
    return HardwareInfo;
}

bool SOmniCaptureNVENCStatus::ScanForNVENCLibrary()
{
    FOmniCaptureNVENCAPI& NVENCAPI = FOmniCaptureNVENCAPI::Get();
    
    // 卸载当前加载的库
    NVENCAPI.UnloadNVEncodeAPI();
    
    // 重新扫描
    bool bFound = NVENCAPI.ScanSystemForNVEncodeAPI();
    
    // 尝试加载
    if (bFound)
    {
        bFound = NVENCAPI.LoadNVEncodeAPI();
    }
    
    // 刷新状态
    RefreshNVENCStatus();
    
    return bFound;
}

bool SOmniCaptureNVENCStatus::SetNVENCLibraryPath(const FString& InLibraryPath)
{
    FOmniCaptureNVENCAPI& NVENCAPI = FOmniCaptureNVENCAPI::Get();
    
    // 设置路径
    bool bSuccess = NVENCAPI.SetNVEncodeAPILibraryPath(InLibraryPath);
    
    // 尝试加载
    if (bSuccess)
    {
        bSuccess = NVENCAPI.LoadNVEncodeAPI();
    }
    
    // 刷新状态
    RefreshNVENCStatus();
    
    return bSuccess;
}

void SOmniCaptureNVENCStatus::StartAutoRefresh(float Interval)
{
    // 停止之前的定时器
    StopAutoRefresh();
    
    // 设置新的间隔
    RefreshInterval = Interval;
    bAutoRefresh = true;
    
    // 创建新的定时器
    if (GEditor && GEditor->GetTimerManager())
    {
        FTimerHandle NewTimerHandle;
        GEditor->GetTimerManager()->SetTimer(NewTimerHandle, FTimerDelegate::CreateSP(this, &SOmniCaptureNVENCStatus::OnRefreshTimer), RefreshInterval, true);
        RefreshTimerHandle = NewTimerHandle;
    }
}

void SOmniCaptureNVENCStatus::StopAutoRefresh()
{
    bAutoRefresh = false;
    
    // 清除定时器
    if (GEditor && GEditor->GetTimerManager() && RefreshTimerHandle.IsValid())
    {
        GEditor->GetTimerManager()->ClearTimer(RefreshTimerHandle.Pin().Get());
    }
    RefreshTimerHandle.Reset();
}

FString SOmniCaptureNVENCStatus::GetStatusText() const
{
    if (bNVENCAvailable)
    {
        return TEXT("NVENC Available and Ready");
    }
    else if (!LibraryPath.IsEmpty())
    {
        return TEXT("NVENC Library Found but Not Available");
    }
    else
    {
        return TEXT("NVENC Not Available");
    }
}

FSlateColor SOmniCaptureNVENCStatus::GetStatusColor() const
{
    if (bNVENCAvailable)
    {
        return FSlateColor(FColor::Green);
    }
    else if (!LibraryPath.IsEmpty())
    {
        return FSlateColor(FColor::Yellow);
    }
    else
    {
        return FSlateColor(FColor::Red);
    }
}

FString SOmniCaptureNVENCStatus::GetDetailedInfo() const
{
    if (bNVENCAvailable)
    {
        return FString::Printf(TEXT("Hardware: %s"), *HardwareInfo);
    }
    else if (!LibraryPath.IsEmpty())
    {
        return TEXT("NVENC library found but GPU may not support encoding or drivers need updating");
    }
    else
    {
        return TEXT("Please ensure NVIDIA drivers are installed and up-to-date");
    }
}

void SOmniCaptureNVENCStatus::OnRefreshTimer()
{
    RefreshNVENCStatus();
}

void SOmniCaptureNVENCStatus::UpdateUIDisplay()
{
    if (DriverVersionTextBlock)
    {
        DriverVersionTextBlock->SetText(FText::Format(LOCTEXT("DriverVersionFormat", "Driver Version: {0}"), 
            FText::FromString(DriverVersion.IsEmpty() ? TEXT("Unknown") : DriverVersion)));
    }
    
    if (SessionsTextBlock)
    {
        SessionsTextBlock->SetText(FText::Format(LOCTEXT("SessionsFormat", "Max Encoder Sessions: {0}"), 
            FText::AsNumber(MaxEncoderSessions)));
    }
    
    if (LibraryPathTextBlock)
    {
        LibraryPathTextBlock->SetText(FText::Format(LOCTEXT("LibraryPathFormat", "Library Path: {0}"), 
            FText::FromString(LibraryPath.IsEmpty() ? TEXT("Not Found") : LibraryPath)));
    }
}

const FSlateBrush* SOmniCaptureNVENCStatus::GetStatusIcon() const
{
    if (bNVENCAvailable)
    {
        return FEditorStyle::GetBrush("Icons.Success");
    }
    else if (!LibraryPath.IsEmpty())
    {
        return FEditorStyle::GetBrush("Icons.Warning");
    }
    else
    {
        return FEditorStyle::GetBrush("Icons.Error");
    }
}

void SOmniCaptureNVENCStatus::OnBrowseNVENCLibrary()
{
    // 打开文件选择对话框
    TSharedPtr<SFileDialog> FileDialog = SNew(SFileDialog);
    
    // 设置过滤器
    FileDialog->SetFileTypes(
    {
        { TEXT("NVENC API Library"), TEXT("*.dll") },
        { TEXT("All Files"), TEXT("*") }
    });
    
    // 设置默认路径
    if (!LibraryPath.IsEmpty())
    {
        FileDialog->SetDefaultDirectory(FPaths::GetPath(LibraryPath));
    }
    
    // 设置标题
    FileDialog->SetTitle(LOCTEXT("SelectNVENCLibraryTitle", "Select NVENC API Library"));
    
    // 显示对话框
    if (FileDialog->ShowModal())
    {
        FString SelectedPath = FileDialog->GetSelectedFilename();
        if (!SelectedPath.IsEmpty())
        {
            // 尝试设置路径
            if (SetNVENCLibraryPath(SelectedPath))
            {
                FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("LibrarySetSuccess", "NVENC library path set successfully!"));
            }
            else
            {
                FMessageDialog::Open(EAppMsgType::Warning, LOCTEXT("LibrarySetFailed", "Failed to set NVENC library path!"));
            }
        }
    }
}

void SOmniCaptureNVENCStatus::OnResetToDefault()
{
    // 卸载当前加载的库
    FOmniCaptureNVENCAPI& NVENCAPI = FOmniCaptureNVENCAPI::Get();
    NVENCAPI.UnloadNVEncodeAPI();
    
    // 清除状态
    bNVENCAvailable = false;
    DriverVersion.Empty();
    LibraryPath.Empty();
    MaxEncoderSessions = 0;
    HardwareInfo.Empty();
    
    // 更新UI
    UpdateUIDisplay();
    
    // 提示用户
    FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("ResetToDefaultSuccess", "Reset to default NVENC settings!"));
}

void SOmniCaptureNVENCStatus::OnTestNVENCFunctionality()
{
    // 运行NVENC功能测试
    FOmniCaptureNVENCAPI& NVENCAPI = FOmniCaptureNVENCAPI::Get();
    
    bool bTestPassed = NVENCAPI.RunNVENCAvailabilityTest();
    
    if (bTestPassed)
    {
        // 生成详细的测试报告
        FString Report = FOmniCaptureNVENCDeviceDetector::GenerateDiagnosticReport();
        
        // 显示成功消息和报告
        FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
            LOCTEXT("TestPassed", "NVENC functionality test passed!\n\n{0}"),
            FText::FromString(Report)
        ));
    }
    else
    {
        FMessageDialog::Open(EAppMsgType::Warning, LOCTEXT("TestFailed", "NVENC functionality test failed! Please check your NVIDIA drivers and hardware."));
    }
    
    // 刷新状态
    RefreshNVENCStatus();
}

// FOmniCaptureNVENCDeviceDetector 实现

FOmniCaptureNVENCDeviceInfo FOmniCaptureNVENCDeviceDetector::GetDeviceInfo()
{
    FOmniCaptureNVENCDeviceInfo DeviceInfo;
    FOmniCaptureNVENCAPI& NVENCAPI = FOmniCaptureNVENCAPI::Get();
    
    // 确保NVENC API已加载
    if (!NVENCAPI.IsNVEncodeAPILoaded())
    {
        NVENCAPI.LoadNVEncodeAPI();
    }
    
    // 填充设备信息
    DeviceInfo.bAvailable = NVENCAPI.IsNVENCAvailable();
    DeviceInfo.DriverVersion = NVENCAPI.GetNVENCDriverVersion();
    DeviceInfo.LibraryPath = NVENCAPI.GetNVEncodeAPILibraryPath();
    DeviceInfo.MaxEncoderSessions = NVENCAPI.GetMaxEncoderSessions();
    DeviceInfo.HardwareInfo = NVENCAPI.GetNVENCHardwareInfo();
    
    // 获取支持的编码器和分辨率
    if (DeviceInfo.bAvailable)
    {
        DeviceInfo.SupportedEncoders = GetSupportedEncoders();
        DeviceInfo.SupportedResolutions = GetSupportedResolutions();
        DeviceInfo.MaxSupportedFrameRate = 240.0f; // 假设大多数GPU支持到240fps
    }
    
    return DeviceInfo;
}

bool FOmniCaptureNVENCDeviceDetector::ScanSystemDevices()
{
    FOmniCaptureNVENCAPI& NVENCAPI = FOmniCaptureNVENCAPI::Get();
    
    // 扫描系统中的NVENC库
    bool bFound = NVENCAPI.ScanSystemForNVEncodeAPI();
    
    // 如果找到，尝试加载
    if (bFound)
    {
        bFound = NVENCAPI.LoadNVEncodeAPI();
    }
    
    return bFound && NVENCAPI.IsNVENCAvailable();
}

bool FOmniCaptureNVENCDeviceDetector::RunFunctionalityTest()
{
    FOmniCaptureNVENCAPI& NVENCAPI = FOmniCaptureNVENCAPI::Get();
    return NVENCAPI.RunNVENCAvailabilityTest();
}

TArray<FString> FOmniCaptureNVENCDeviceDetector::GetSupportedEncoders()
{
    // 简化实现，实际应该查询NVENC API获取支持的编码器
    TArray<FString> SupportedEncoders;
    
    // 假设现代NVIDIA GPU至少支持H.264
    SupportedEncoders.Add(TEXT("H264"));
    
    // 较新的GPU支持H.265
    SupportedEncoders.Add(TEXT("H265/HEVC"));
    
    // 最新的GPU可能支持AV1
    // 实际应用中应该通过NVENC API检查
    
    return SupportedEncoders;
}

TArray<FIntPoint> FOmniCaptureNVENCDeviceDetector::GetSupportedResolutions()
{
    // 返回常见的支持分辨率列表
    // 实际应该查询NVENC API获取硬件支持的最大分辨率
    return SupportedResolutionsList;
}

bool FOmniCaptureNVENCDeviceDetector::IsResolutionSupported(const FIntPoint& Resolution)
{
    // 简化实现，检查是否小于等于4K
    // 实际应该查询NVENC API获取硬件支持的最大分辨率
    return (Resolution.X <= 3840 && Resolution.Y <= 2160);
}

bool FOmniCaptureNVENCDeviceDetector::IsEncoderSupported(const FString& EncoderName)
{
    TArray<FString> SupportedEncoders = GetSupportedEncoders();
    return SupportedEncoders.Contains(EncoderName);
}

FString FOmniCaptureNVENCDeviceDetector::GetOptimalConfigurationRecommendation()
{
    FOmniCaptureNVENCDeviceInfo DeviceInfo = GetDeviceInfo();
    
    if (!DeviceInfo.bAvailable)
    {
        return TEXT("NVENC not available. Cannot provide recommendation.");
    }
    
    FString Recommendation = TEXT("Optimal Configuration Recommendation:\n");
    Recommendation += FString::Printf(TEXT("- Encoder: %s\n"), *DeviceInfo.SupportedEncoders[0]);
    
    // 根据硬件能力推荐分辨率
    if (DeviceInfo.SupportedResolutions.Contains(FIntPoint(3840, 2160)))
    {
        Recommendation += TEXT("- Resolution: 3840x2160 (4K) or lower\n");
    }
    else if (DeviceInfo.SupportedResolutions.Contains(FIntPoint(1920, 1080)))
    {
        Recommendation += TEXT("- Resolution: 1920x1080 (1080p) or lower\n");
    }
    
    Recommendation += TEXT("- Frame Rate: 60fps (balancing quality and performance)\n");
    Recommendation += TEXT("- Bitrate: 15-20 Mbps for 1080p, 40-50 Mbps for 4K");
    
    return Recommendation;
}

FString FOmniCaptureNVENCDeviceDetector::GenerateDiagnosticReport()
{
    FOmniCaptureNVENCDeviceInfo DeviceInfo = GetDeviceInfo();
    
    FString Report = TEXT("NVENC Diagnostic Report:\n\n");
    
    // 基本信息
    Report += TEXT("=== Basic Information ===\n");
    Report += FString::Printf(TEXT("Available: %s\n"), DeviceInfo.bAvailable ? TEXT("Yes") : TEXT("No"));
    Report += FString::Printf(TEXT("Driver Version: %s\n"), *DeviceInfo.DriverVersion);
    Report += FString::Printf(TEXT("Library Path: %s\n"), *DeviceInfo.LibraryPath);
    Report += FString::Printf(TEXT("Hardware: %s\n"), *DeviceInfo.HardwareInfo);
    Report += FString::Printf(TEXT("Max Encoder Sessions: %d\n\n"), DeviceInfo.MaxEncoderSessions);
    
    // 支持的功能
    if (DeviceInfo.bAvailable)
    {
        Report += TEXT("=== Supported Features ===\n");
        
        // 编码器
        Report += TEXT("Encoders:\n");
        for (const FString& Encoder : DeviceInfo.SupportedEncoders)
        {
            Report += FString::Printf(TEXT("  - %s\n"), *Encoder);
        }
        
        // 分辨率
        Report += TEXT("\nCommon Supported Resolutions:\n");
        for (const FIntPoint& Res : DeviceInfo.SupportedResolutions)
        {
            if (Res.X <= 3840) // 只显示到4K
            {
                Report += FString::Printf(TEXT("  - %dx%d\n"), Res.X, Res.Y);
            }
        }
        
        // 最大帧率
        Report += FString::Printf(TEXT("\nMax Supported Frame Rate: %.0ffps\n\n"), DeviceInfo.MaxSupportedFrameRate);
        
        // 建议配置
        Report += TEXT("=== Recommended Configuration ===\n");
        Report += GetOptimalConfigurationRecommendation();
    }
    else
    {
        Report += TEXT("=== Troubleshooting Tips ===\n");
        Report += TEXT("1. Ensure you have NVIDIA drivers installed and up-to-date\n");
        Report += TEXT("2. Verify your GPU supports NVENC encoding (most NVIDIA GPUs from Fermi generation onwards)\n");
        Report += TEXT("3. Try manually selecting the NVENC library path\n");
        Report += TEXT("4. Check if your GPU is being used by another application for encoding\n");
    }
    
    return Report;
}

#undef LOCTEXT_NAMESPACE
