// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DetailLayoutBuilder.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorUndoClient.h"

/**
 * NVENC编码器设置的细节面板定制
 */
class FOmniCaptureNVENCSettingDetails : public IDetailCustomization, public FEditorUndoClient
{
public:
    /** 创建实例 */
    static TSharedRef<IDetailCustomization> MakeInstance();

    // 从IDetailCustomization继承的方法
    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

    // 从FEditorUndoClient继承的方法
    virtual void PostUndo(bool bSuccess) override;
    virtual void PostRedo(bool bSuccess) override;

private:
    /** 构造函数 */
    FOmniCaptureNVENCSettingDetails();
    
    /** 析构函数 */
    virtual ~FOmniCaptureNVENCSettingDetails();

    /** 刷新细节面板 */
    void RefreshDetails();

    /** 验证配置有效性 */
    void ValidateConfiguration();

    /** 获取可用的编解码器选项 */
    void UpdateAvailableCodecs();

    /** 获取可用的色彩格式选项 */
    void UpdateAvailableColorFormats();

    /** 根据质量预设更新UI */
    void OnQualityPresetChanged();

    /** 启用/禁用高级设置 */
    void ToggleAdvancedSettings();

    /** 重置为默认配置 */
    void ResetToDefault();

    /** 测试NVENC可用性 */
    void TestNVENCAvailability();

    /** 显示警告消息 */
    void ShowWarningMessage(const FText& Message);

    /** 显示错误消息 */
    void ShowErrorMessage(const FText& Message);

private:
    /** 细节布局构建器 */
    TWeakPtr<IDetailLayoutBuilder> DetailBuilderPtr;

    /** 是否启用高级设置 */
    bool bShowAdvancedSettings;

    /** NVENC可用性状态 */
    bool bNVENCAvailable;

    /** 可用编解码器列表 */
    TArray<TSharedPtr<FString>> AvailableCodecs;
    TSharedPtr<FString> SelectedCodec;

    /** 可用色彩格式列表 */
    TArray<TSharedPtr<FString>> AvailableColorFormats;
    TSharedPtr<FString> SelectedColorFormat;

    /** 上次选择的质量预设 */
    EOmniCaptureQualityPreset LastSelectedQualityPreset;

    /** 验证错误消息 */
    FText ValidationErrorText;

    /** 验证警告消息 */
    FText ValidationWarningText;
};

/**
 * NVENC编码器设置面板
 */
class SOmniCaptureNVENCSettingPanel : public SCompoundWidget
{
    SLATE_BEGIN_ARGS(SOmniCaptureNVENCSettingPanel)
    {}
    SLATE_END_ARGS()

public:
    /** 构造函数 */
    void Construct(const FArguments& InArgs);

private:
    /** 更新可用设备列表 */
    void UpdateAvailableDevices();

    /** 获取当前选中的设备 */
    TSharedPtr<FString> GetCurrentDevice() const;

    /** 设备选择改变 */
    void OnDeviceSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);

    /** 获取设备选择器的文本 */
    TArray<TSharedPtr<FString>> GetDeviceOptions() const;

    /** 设备选择器生成器 */
    TSharedRef<SWidget> MakeDeviceComboWidget(TSharedPtr<FString> InItem);

    /** 刷新设备信息 */
    void RefreshDeviceInfo();

    /** 显示设备信息 */
    FText GetDeviceInfoText() const;

    /** 获取设备状态图标 */
    const FSlateBrush* GetDeviceStatusIcon() const;

private:
    /** 设备组合框 */
    TSharedPtr<SComboBox<TSharedPtr<FString>>> DeviceComboBox;

    /** 可用设备列表 */
    TArray<TSharedPtr<FString>> AvailableDevices;

    /** 设备信息文本 */
    FText DeviceInfoText;

    /** 设备状态 */
    bool bDeviceAvailable;
};