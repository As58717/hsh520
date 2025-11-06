// Copyright Epic Games, Inc. All Rights Reserved.

#include "OmniCaptureNVENCSettingDetails.h"
#include "OmniCaptureNVENCConfig.h"
#include "OmniCaptureNVENCEncoderDirect.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailsView.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "OmniCaptureNVENCSettingDetails"

TSharedRef<IDetailCustomization> FOmniCaptureNVENCSettingDetails::MakeInstance()
{
    return MakeShareable(new FOmniCaptureNVENCSettingDetails());
}

FOmniCaptureNVENCSettingDetails::FOmniCaptureNVENCSettingDetails()
    : bShowAdvancedSettings(false)
    , bNVENCAvailable(false)
    , LastSelectedQualityPreset(EOmniCaptureQualityPreset::Balanced)
{}

FOmniCaptureNVENCSettingDetails::~FOmniCaptureNVENCSettingDetails()
{
    // 注销撤销客户端
    if (GEditor)
    {
        GEditor->GetEditorUndoClient().Unregister(this);
    }
}

void FOmniCaptureNVENCSettingDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    DetailBuilderPtr = &DetailBuilder;

    // 注册撤销客户端
    if (GEditor)
    {
        GEditor->GetEditorUndoClient().Register(this);
    }

    // 测试NVENC可用性
    TestNVENCAvailability();

    // 获取配置对象
    TArray<TWeakObjectPtr<UObject>> SelectedObjects;
    DetailBuilder.GetObjectsBeingCustomized(SelectedObjects);

    if (SelectedObjects.Num() == 1)
    {
        UOmniCaptureNVENCConfig* Config = Cast<UOmniCaptureNVENCConfig>(SelectedObjects[0].Get());
        if (Config)
        {
            LastSelectedQualityPreset = Config->QualityPreset;
        }
    }

    // 创建基本编码设置分类
    IDetailCategoryBuilder& BasicCategory = DetailBuilder.EditCategory("Basic Encoding Settings", LOCTEXT("BasicEncodingSettings", "基本编码设置"));
    
    // 创建高级设置分类
    IDetailCategoryBuilder& AdvancedCategory = DetailBuilder.EditCategory("Advanced Encoding Settings", LOCTEXT("AdvancedEncodingSettings", "高级编码设置"));
    
    // 创建色彩设置分类
    IDetailCategoryBuilder& ColorCategory = DetailBuilder.EditCategory("Color Settings", LOCTEXT("ColorSettings", "色彩设置"));
    
    // 创建诊断设置分类
    IDetailCategoryBuilder& DiagnosticsCategory = DetailBuilder.EditCategory("Diagnostics", LOCTEXT("Diagnostics", "诊断"));

    // 添加控制按钮
    BasicCategory.AddCustomRow(LOCTEXT("Controls", "控制"))
        .WholeRowContent()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(5)
            [
                SNew(SButton)
                .Text(LOCTEXT("AdvancedSettings", "高级设置"))
                .ToolTipText(LOCTEXT("AdvancedSettingsTooltip", "显示/隐藏高级设置选项"))
                .OnClicked_Lambda([this]() {
                    ToggleAdvancedSettings();
                    return FReply::Handled();
                })
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(5)
            [
                SNew(SButton)
                .Text(LOCTEXT("ResetToDefault", "重置为默认"))
                .ToolTipText(LOCTEXT("ResetToDefaultTooltip", "将所有设置重置为默认值"))
                .OnClicked_Lambda([this]() {
                    ResetToDefault();
                    return FReply::Handled();
                })
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(5)
            [
                SNew(SButton)
                .Text(LOCTEXT("TestNVENC", "测试NVENC"))
                .ToolTipText(LOCTEXT("TestNVENCTooltip", "测试NVENC编码器是否可用"))
                .OnClicked_Lambda([this]() {
                    TestNVENCAvailability();
                    return FReply::Handled();
                })
            ]
        ];

    // 添加NVENC状态显示
    BasicCategory.AddCustomRow(LOCTEXT("NVENCStatus", "NVENC状态"))
        .WholeRowContent()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(5)
            [
                SNew(SImage)
                .Image_Lambda([this]() {
                    return bNVENCAvailable ? FEditorStyle::GetBrush("Icons.Success") : FEditorStyle::GetBrush("Icons.Error");
                })
                .ToolTipText_Lambda([this]() {
                    return bNVENCAvailable ? LOCTEXT("NVENCAvailable", "NVENC可用") : LOCTEXT("NVENCUnavailable", "NVENC不可用");
                })
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text_Lambda([this]() {
                    return bNVENCAvailable ? LOCTEXT("NVENCAvailableText", "NVENC硬件编码器可用") : LOCTEXT("NVENCUnavailableText", "NVENC硬件编码器不可用，请确保您有支持NVENC的NVIDIA显卡");
                })
                .ColorAndOpacity_Lambda([this]() {
                    return bNVENCAvailable ? FSlateColor(FLinearColor::Green) : FSlateColor(FLinearColor::Red);
                })
            ]
        ];

    // 添加设备选择面板
    BasicCategory.AddCustomRow(LOCTEXT("DeviceSelection", "设备选择"))
        .WholeRowContent()
        [
            SNew(SOmniCaptureNVENCSettingPanel)
        ];

    // 初始化编解码器选项
    UpdateAvailableCodecs();

    // 初始化色彩格式选项
    UpdateAvailableColorFormats();

    // 监听质量预设变化
    DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UOmniCaptureNVENCConfig, QualityPreset))->SetOnPropertyValueChanged_Lambda([this]() {
        OnQualityPresetChanged();
    });

    // 初始验证
    ValidateConfiguration();
}

void FOmniCaptureNVENCSettingDetails::PostUndo(bool bSuccess)
{
    RefreshDetails();
}

void FOmniCaptureNVENCSettingDetails::PostRedo(bool bSuccess)
{
    RefreshDetails();
}

void FOmniCaptureNVENCSettingDetails::RefreshDetails()
{
    if (DetailBuilderPtr.IsValid())
    {
        DetailBuilderPtr.Pin()->ForceRefreshDetails();
    }
    
    ValidateConfiguration();
}

void FOmniCaptureNVENCSettingDetails::ValidateConfiguration()
{
    TArray<TWeakObjectPtr<UObject>> SelectedObjects;
    if (DetailBuilderPtr.IsValid())
    {
        DetailBuilderPtr.Pin()->GetObjectsBeingCustomized(SelectedObjects);
        
        if (SelectedObjects.Num() == 1)
        {
            UOmniCaptureNVENCConfig* Config = Cast<UOmniCaptureNVENCConfig>(SelectedObjects[0].Get());
            if (Config)
            {
                if (!Config->IsValidConfig())
                {
                    ShowErrorMessage(LOCTEXT("InvalidConfig", "配置无效，请检查设置"));
                }
                else
                {
                    // 获取NVENC能力并验证配置
                    auto Capabilities = FOmniCaptureNVENCEncoderDirect::GetNVENCCapabilities();
                    
                    // 这里可以添加更多验证逻辑
                    
                    if (!bNVENCAvailable)
                    {
                        ShowWarningMessage(LOCTEXT("NVENCNotAvailable", "NVENC不可用，配置将在可用时应用"));
                    }
                }
            }
        }
    }
}

void FOmniCaptureNVENCSettingDetails::UpdateAvailableCodecs()
{
    AvailableCodecs.Empty();
    
    // 获取NVENC能力
    auto Capabilities = FOmniCaptureNVENCEncoderDirect::GetNVENCCapabilities();
    
    if (Capabilities.bSupportsH264)
    {
        AvailableCodecs.Add(MakeShareable(new FString(TEXT("H264"))));
    }
    
    if (Capabilities.bSupportsHEVC)
    {
        AvailableCodecs.Add(MakeShareable(new FString(TEXT("HEVC"))));
    }
    
    if (AvailableCodecs.Num() > 0)
    {
        SelectedCodec = AvailableCodecs[0];
    }
}

void FOmniCaptureNVENCSettingDetails::UpdateAvailableColorFormats()
{
    AvailableColorFormats.Empty();
    
    // 获取NVENC能力
    auto Capabilities = FOmniCaptureNVENCEncoderDirect::GetNVENCCapabilities();
    
    if (Capabilities.bSupportsNV12)
    {
        AvailableColorFormats.Add(MakeShareable(new FString(TEXT("NV12"))));
    }
    
    if (Capabilities.bSupportsP010)
    {
        AvailableColorFormats.Add(MakeShareable(new FString(TEXT("P010"))));
    }
    
    if (Capabilities.bSupportsBGRA)
    {
        AvailableColorFormats.Add(MakeShareable(new FString(TEXT("BGRA"))));
    }
    
    if (AvailableColorFormats.Num() > 0)
    {
        SelectedColorFormat = AvailableColorFormats[0];
    }
}

void FOmniCaptureNVENCSettingDetails::OnQualityPresetChanged()
{
    TArray<TWeakObjectPtr<UObject>> SelectedObjects;
    if (DetailBuilderPtr.IsValid())
    {
        DetailBuilderPtr.Pin()->GetObjectsBeingCustomized(SelectedObjects);
        
        if (SelectedObjects.Num() == 1)
        {
            UOmniCaptureNVENCConfig* Config = Cast<UOmniCaptureNVENCConfig>(SelectedObjects[0].Get());
            if (Config && Config->QualityPreset != LastSelectedQualityPreset)
            {
                // 应用新的质量预设
                Config->ApplyQualityPreset(Config->QualityPreset);
                LastSelectedQualityPreset = Config->QualityPreset;
                
                // 刷新UI
                DetailBuilderPtr.Pin()->ForceRefreshDetails();
            }
        }
    }
}

void FOmniCaptureNVENCSettingDetails::ToggleAdvancedSettings()
{
    bShowAdvancedSettings = !bShowAdvancedSettings;
    
    if (DetailBuilderPtr.IsValid())
    {
        // 显示或隐藏高级设置分类
        IDetailCategoryBuilder* AdvancedCategory = DetailBuilderPtr.Pin()->FindCategory("Advanced Encoding Settings");
        if (AdvancedCategory)
        {
            AdvancedCategory->SetCategoryVisibility(bShowAdvancedSettings);
        }
    }
}

void FOmniCaptureNVENCSettingDetails::ResetToDefault()
{
    TArray<TWeakObjectPtr<UObject>> SelectedObjects;
    if (DetailBuilderPtr.IsValid())
    {
        DetailBuilderPtr.Pin()->GetObjectsBeingCustomized(SelectedObjects);
        
        for (auto& ObjectPtr : SelectedObjects)
        {
            UOmniCaptureNVENCConfig* Config = Cast<UOmniCaptureNVENCConfig>(ObjectPtr.Get());
            if (Config)
            {
                // 获取默认配置
                UOmniCaptureNVENCConfig* DefaultConfig = UOmniCaptureNVENCConfig::GetDefault();
                if (DefaultConfig)
                {
                    // 复制默认值
                    Config->Codec = DefaultConfig->Codec;
                    Config->QualityPreset = DefaultConfig->QualityPreset;
                    Config->TargetBitrateKbps = DefaultConfig->TargetBitrateKbps;
                    Config->MaxBitrateKbps = DefaultConfig->MaxBitrateKbps;
                    Config->GOPSize = DefaultConfig->GOPSize;
                    Config->BFrameCount = DefaultConfig->BFrameCount;
                    Config->bUseCBR = DefaultConfig->bUseCBR;
                    Config->bEnableCUDA = DefaultConfig->bEnableCUDA;
                    Config->bEnableDynamicGOP = DefaultConfig->bEnableDynamicGOP;
                    Config->bUseSceneChangeDetection = DefaultConfig->bUseSceneChangeDetection;
                    Config->MaxEncodingLatencyMs = DefaultConfig->MaxEncodingLatencyMs;
                    Config->EncodingThreadPriority = DefaultConfig->EncodingThreadPriority;
                    Config->ColorFormat = DefaultConfig->ColorFormat;
                    Config->bEnableHDR = DefaultConfig->bEnableHDR;
                    Config->ColorSpace = DefaultConfig->ColorSpace;
                    Config->ColorRange = DefaultConfig->ColorRange;
                    Config->bEnableDiagnostics = DefaultConfig->bEnableDiagnostics;
                    Config->bLogEncodingStats = DefaultConfig->bLogEncodingStats;
                    Config->StatsLogInterval = DefaultConfig->StatsLogInterval;
                    
                    LastSelectedQualityPreset = Config->QualityPreset;
                }
            }
        }
        
        DetailBuilderPtr.Pin()->ForceRefreshDetails();
    }
    
    ShowWarningMessage(LOCTEXT("ResetToDefaultMessage", "配置已重置为默认值"));
}

void FOmniCaptureNVENCSettingDetails::TestNVENCAvailability()
{
    bNVENCAvailable = FOmniCaptureNVENCEncoderDirect::IsNVENCAvailable();
    
    if (bNVENCAvailable)
    {
        ShowWarningMessage(LOCTEXT("NVENCAvailableMessage", "NVENC编码器可用"));
    }
    else
    {
        ShowErrorMessage(LOCTEXT("NVENCUnavailableMessage", "NVENC编码器不可用，请确保您有支持NVENC的NVIDIA显卡并且驱动程序已更新"));
    }
    
    // 更新可用选项
    UpdateAvailableCodecs();
    UpdateAvailableColorFormats();
    
    RefreshDetails();
}

void FOmniCaptureNVENCSettingDetails::ShowWarningMessage(const FText& Message)
{
    FNotificationInfo Info(Message);
    Info.bUseSuccessFailIcons = true;
    Info.FadeOutDuration = 3.0f;
    
    FSlateNotificationManager::Get().AddNotification(Info);
}

void FOmniCaptureNVENCSettingDetails::ShowErrorMessage(const FText& Message)
{
    FNotificationInfo Info(Message);
    Info.bUseSuccessFailIcons = true;
    Info.FadeOutDuration = 5.0f;
    Info.ExpireDuration = 8.0f;
    Info.bFireAndForget = false;
    
    FSlateNotificationManager::Get().AddNotification(Info);
}

// SOmniCaptureNVENCSettingPanel实现
void SOmniCaptureNVENCSettingPanel::Construct(const FArguments& InArgs)
{
    bDeviceAvailable = false;
    DeviceInfoText = LOCTEXT("NoDeviceInfo", "未检测到NVENC设备信息");
    
    // 更新可用设备
    UpdateAvailableDevices();
    
    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(5)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(5)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("SelectDevice", "选择设备:"))
                .Font(FAppStyle::GetFontStyle("NormalFont"))
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            .Padding(5)
            [
                SAssignNew(DeviceComboBox, SComboBox<TSharedPtr<FString>>)
                .OptionsSource(&AvailableDevices)
                .OnSelectionChanged(this, &SOmniCaptureNVENCSettingPanel::OnDeviceSelectionChanged)
                .OnGenerateWidget(this, &SOmniCaptureNVENCSettingPanel::MakeDeviceComboWidget)
                .InitiallySelectedItem(GetCurrentDevice())
                [
                    SNew(STextBlock)
                    .Text(this, &SOmniCaptureNVENCSettingPanel::GetCurrentDevice, true)
                ]
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(5)
            [
                SNew(SButton)
                .Text(LOCTEXT("Refresh", "刷新"))
                .ToolTipText(LOCTEXT("RefreshTooltip", "刷新设备列表"))
                .OnClicked_Lambda([this]() {
                    RefreshDeviceInfo();
                    return FReply::Handled();
                })
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(5)
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
            .Padding(8)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Top)
                .Padding(5)
                [
                    SNew(SImage)
                    .Image(this, &SOmniCaptureNVENCSettingPanel::GetDeviceStatusIcon)
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .Padding(5)
                [
                    SNew(STextBlock)
                    .Text(this, &SOmniCaptureNVENCSettingPanel::GetDeviceInfoText)
                    .AutoWrapText(true)
                ]
            ]
        ]
    ];
}

void SOmniCaptureNVENCSettingPanel::UpdateAvailableDevices()
{
    AvailableDevices.Empty();
    
    // 检查NVENC是否可用
    bool bNVENCAvailable = FOmniCaptureNVENCEncoderDirect::IsNVENCAvailable();
    
    if (bNVENCAvailable)
    {
        // 这里应该获取实际的设备列表，但为了简化，我们只添加一个默认设备
        // 在实际实现中，应该使用NVENC API枚举所有可用设备
        AvailableDevices.Add(MakeShareable(new FString(TEXT("NVIDIA GPU (默认)"))));
        bDeviceAvailable = true;
        DeviceInfoText = LOCTEXT("DeviceAvailableInfo", "NVIDIA GPU支持NVENC硬件编码");
    }
    else
    {
        AvailableDevices.Add(MakeShareable(new FString(TEXT("无可用设备"))));
        bDeviceAvailable = false;
        DeviceInfoText = LOCTEXT("NoAvailableDevice", "未检测到支持NVENC的NVIDIA GPU");
    }
}

TSharedPtr<FString> SOmniCaptureNVENCSettingPanel::GetCurrentDevice() const
{
    if (AvailableDevices.Num() > 0)
    {
        return AvailableDevices[0];
    }
    return MakeShareable(new FString(TEXT("无可用设备")));
}

void SOmniCaptureNVENCSettingPanel::OnDeviceSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
    // 设备选择改变时的处理逻辑
    if (NewSelection.IsValid())
    {
        // 这里可以添加设备选择后的处理
        UE_LOG(LogTemp, Log, TEXT("Selected device: %s"), **NewSelection);
    }
}

TArray<TSharedPtr<FString>> SOmniCaptureNVENCSettingPanel::GetDeviceOptions() const
{
    return AvailableDevices;
}

TSharedRef<SWidget> SOmniCaptureNVENCSettingPanel::MakeDeviceComboWidget(TSharedPtr<FString> InItem)
{
    if (!InItem.IsValid())
    {
        return SNullWidget::NullWidget;
    }
    
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(5)
        [
            SNew(SImage)
            .Image_Lambda([this]() {
                return bDeviceAvailable ? FEditorStyle::GetBrush("Icons.Success") : FEditorStyle::GetBrush("Icons.Error");
            })
        ]
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text(FText::FromString(*InItem))
        ];
}

void SOmniCaptureNVENCSettingPanel::RefreshDeviceInfo()
{
    UpdateAvailableDevices();
    
    if (DeviceComboBox.IsValid())
    {
        DeviceComboBox->RefreshOptions();
        DeviceComboBox->SetSelectedItem(GetCurrentDevice());
    }
}

FText SOmniCaptureNVENCSettingPanel::GetDeviceInfoText() const
{
    return DeviceInfoText;
}

const FSlateBrush* SOmniCaptureNVENCSettingPanel::GetDeviceStatusIcon() const
{
    return bDeviceAvailable ? FEditorStyle::GetBrush("Icons.Success") : FEditorStyle::GetBrush("Icons.Error");
}

#undef LOCTEXT_NAMESPACE