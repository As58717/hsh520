// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OmniCaptureTestUI.generated.h"

class AOmniCaptureTestActor;
class UButton;
class UTextBlock;
class UComboBoxString;
class USpinBox;
class UCheckBox;
class UProgressBar;
class UVerticalBox;

/**
 * 测试UI类，用于控制和监控NVENC编码捕获
 */
UCLASS(Blueprintable, BlueprintType)
class OMNICAPTURE_API UOmniCaptureTestUI : public UUserWidget
{
    GENERATED_BODY()
    
public:
    // 构造函数
    UOmniCaptureTestUI(const FObjectInitializer& ObjectInitializer);
    
    // 从UserWidget继承的函数
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    
    // 设置测试Actor引用
    UFUNCTION(BlueprintCallable, Category = "OmniCapture")
    void SetTestActor(AOmniCaptureTestActor* InTestActor);
    
    // 刷新UI状态
    UFUNCTION(BlueprintCallable, Category = "OmniCapture")
    void RefreshUIState();
    
    // 显示消息
    UFUNCTION(BlueprintCallable, Category = "OmniCapture")
    void ShowMessage(const FString& Message, bool bIsError = false);
    
    // 清除消息
    UFUNCTION(BlueprintCallable, Category = "OmniCapture")
    void ClearMessage();
    
protected:
    // UI组件引用
    
    // 控制按钮
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "OmniCapture UI")
    UButton* Button_StartCapture;
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "OmniCapture UI")
    UButton* Button_StopCapture;
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "OmniCapture UI")
    UButton* Button_PauseCapture;
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "OmniCapture UI")
    UButton* Button_ResumeCapture;
    
    // 质量预设选择器
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "OmniCapture UI")
    UComboBoxString* ComboBox_QualityPreset;
    
    // 分辨率设置
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "OmniCapture UI")
    USpinBox* SpinBox_ResolutionX;
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "OmniCapture UI")
    USpinBox* SpinBox_ResolutionY;
    
    // 帧率设置
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "OmniCapture UI")
    USpinBox* SpinBox_FrameRate;
    
    // 持续时间设置
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "OmniCapture UI")
    USpinBox* SpinBox_Duration;
    
    // 选项复选框
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "OmniCapture UI")
    UCheckBox* CheckBox_EnableHDR;
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "OmniCapture UI")
    UCheckBox* CheckBox_ShowStats;
    
    // 状态和统计信息
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "OmniCapture UI")
    UTextBlock* TextBlock_CaptureStatus;
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "OmniCapture UI")
    UTextBlock* TextBlock_EncodingStats;
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "OmniCapture UI")
    UProgressBar* ProgressBar_CaptureDuration;
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "OmniCapture UI")
    UTextBlock* TextBlock_ProgressPercentage;
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "OmniCapture UI")
    UTextBlock* TextBlock_Message;
    
    // 输出文件路径显示
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "OmniCapture UI")
    UTextBlock* TextBlock_OutputFilePath;
    
    // 消息超时时间
    UPROPERTY(EditDefaultsOnly, Category = "OmniCapture UI")
    float MessageTimeout;
    
private:
    // 测试Actor引用
    UPROPERTY()
    AOmniCaptureTestActor* TestActor;
    
    // 消息计时器
    float MessageTimer;
    
    // 捕获开始时间
    float CaptureStartTime;
    
    // 注册UI事件处理程序
    void RegisterButtonEvents();
    void RegisterComboBoxEvents();
    void RegisterSpinBoxEvents();
    void RegisterCheckBoxEvents();
    
    // 按钮点击处理
    UFUNCTION()
    void OnStartCaptureClicked();
    
    UFUNCTION()
    void OnStopCaptureClicked();
    
    UFUNCTION()
    void OnPauseCaptureClicked();
    
    UFUNCTION()
    void OnResumeCaptureClicked();
    
    // 质量预设变更处理
    UFUNCTION()
    void OnQualityPresetChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    
    // 分辨率变更处理
    UFUNCTION()
    void OnResolutionXChanged(float NewValue);
    
    UFUNCTION()
    void OnResolutionYChanged(float NewValue);
    
    // 帧率变更处理
    UFUNCTION()
    void OnFrameRateChanged(float NewValue);
    
    // 持续时间变更处理
    UFUNCTION()
    void OnDurationChanged(float NewValue);
    
    // 选项复选框变更处理
    UFUNCTION()
    void OnEnableHDRChanged(bool bIsChecked);
    
    UFUNCTION()
    void OnShowStatsChanged(bool bIsChecked);
    
    // 更新状态显示
    void UpdateStatusDisplay();
    
    // 更新进度显示
    void UpdateProgressDisplay(float DeltaTime);
    
    // 更新统计信息显示
    void UpdateStatsDisplay();
    
    // 绑定测试Actor事件
    void BindTestActorEvents();
    
    // 测试Actor事件处理
    UFUNCTION()
    void OnCaptureCompleted(const FString& OutputFilePath);
    
    UFUNCTION()
    void OnCaptureError(const FString& ErrorMessage);
    
    UFUNCTION()
    void OnEncodingStatisticsUpdated(float AverageBitrate, float CurrentFPS);
};
