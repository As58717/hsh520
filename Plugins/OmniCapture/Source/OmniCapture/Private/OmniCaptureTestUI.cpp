// Copyright Epic Games, Inc. All Rights Reserved.

#include "OmniCaptureTestUI.h"
#include "OmniCaptureTestActor.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ComboBoxString.h"
#include "Components/SpinBox.h"
#include "Components/CheckBox.h"
#include "Components/ProgressBar.h"
#include "Components/VerticalBox.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogOmniCaptureTestUI, Log, All);

UOmniCaptureTestUI::UOmniCaptureTestUI(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
    , TestActor(nullptr)
    , MessageTimer(0.0f)
    , CaptureStartTime(0.0f)
    , MessageTimeout(5.0f)
{}

void UOmniCaptureTestUI::NativeConstruct()
{
    Super::NativeConstruct();
    
    // 注册UI事件处理程序
    RegisterButtonEvents();
    RegisterComboBoxEvents();
    RegisterSpinBoxEvents();
    RegisterCheckBoxEvents();
    
    // 初始化质量预设组合框
    if (ComboBox_QualityPreset)
    {
        ComboBox_QualityPreset->AddOption(TEXT("Low"));
        ComboBox_QualityPreset->AddOption(TEXT("Balanced"));
        ComboBox_QualityPreset->AddOption(TEXT("High"));
        ComboBox_QualityPreset->AddOption(TEXT("Ultra"));
        ComboBox_QualityPreset->AddOption(TEXT("Lossless"));
        ComboBox_QualityPreset->SetSelectedOption(TEXT("Balanced"));
    }
    
    // 设置默认值
    if (SpinBox_ResolutionX)
    {
        SpinBox_ResolutionX->SetMinValue(320.0f);
        SpinBox_ResolutionX->SetMaxValue(8192.0f);
        SpinBox_ResolutionX->SetValue(1920.0f);
    }
    
    if (SpinBox_ResolutionY)
    {
        SpinBox_ResolutionY->SetMinValue(240.0f);
        SpinBox_ResolutionY->SetMaxValue(8192.0f);
        SpinBox_ResolutionY->SetValue(1080.0f);
    }
    
    if (SpinBox_FrameRate)
    {
        SpinBox_FrameRate->SetMinValue(1.0f);
        SpinBox_FrameRate->SetMaxValue(240.0f);
        SpinBox_FrameRate->SetValue(60.0f);
    }
    
    if (SpinBox_Duration)
    {
        SpinBox_Duration->SetMinValue(0.0f);
        SpinBox_Duration->SetMaxValue(3600.0f);
        SpinBox_Duration->SetValue(0.0f);
    }
    
    // 清除消息
    ClearMessage();
    
    // 刷新UI状态
    RefreshUIState();
    
    UE_LOG(LogOmniCaptureTestUI, Log, TEXT("Test UI constructed"));
}

void UOmniCaptureTestUI::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    
    // 处理消息超时
    if (MessageTimer > 0.0f)
    {
        MessageTimer -= InDeltaTime;
        if (MessageTimer <= 0.0f)
        {
            ClearMessage();
        }
    }
    
    // 更新状态显示
    UpdateStatusDisplay();
    
    // 更新进度显示
    UpdateProgressDisplay(InDeltaTime);
    
    // 更新统计信息显示（如果启用）
    if (CheckBox_ShowStats && CheckBox_ShowStats->IsChecked())
    {
        UpdateStatsDisplay();
    }
}

void UOmniCaptureTestUI::SetTestActor(AOmniCaptureTestActor* InTestActor)
{
    // 解绑之前的事件
    if (TestActor)
    {
        // 解绑代理
        TestActor->OnCaptureCompleted.RemoveDynamic(this, &UOmniCaptureTestUI::OnCaptureCompleted);
        TestActor->OnCaptureError.RemoveDynamic(this, &UOmniCaptureTestUI::OnCaptureError);
        TestActor->OnEncodingStatisticsUpdated.RemoveDynamic(this, &UOmniCaptureTestUI::OnEncodingStatisticsUpdated);
    }
    
    // 设置新的测试Actor
    TestActor = InTestActor;
    
    // 绑定新Actor的事件
    BindTestActorEvents();
    
    // 如果测试Actor有效，同步UI设置
    if (TestActor)
    {
        // 同步分辨率
        FIntPoint Resolution = TestActor->CaptureResolution;
        if (SpinBox_ResolutionX)
        {
            SpinBox_ResolutionX->SetValue(Resolution.X);
        }
        if (SpinBox_ResolutionY)
        {
            SpinBox_ResolutionY->SetValue(Resolution.Y);
        }
        
        // 同步帧率
        if (SpinBox_FrameRate)
        {
            SpinBox_FrameRate->SetValue(TestActor->CaptureFrameRate);
        }
        
        // 同步持续时间
        if (SpinBox_Duration)
        {
            SpinBox_Duration->SetValue(TestActor->CaptureDuration);
        }
        
        // 同步HDR设置
        if (CheckBox_EnableHDR)
        {
            CheckBox_EnableHDR->SetIsChecked(TestActor->bEnableHDR);
        }
        
        // 同步统计显示设置
        if (CheckBox_ShowStats)
        {
            CheckBox_ShowStats->SetIsChecked(TestActor->bShowEncodingStats);
        }
        
        // 更新输出文件路径
        if (TextBlock_OutputFilePath)
        {
            TextBlock_OutputFilePath->SetText(FText::FromString(TestActor->OutputFilePath));
        }
    }
    
    // 刷新UI状态
    RefreshUIState();
    
    UE_LOG(LogOmniCaptureTestUI, Log, TEXT("Test actor set"));
}

void UOmniCaptureTestUI::RefreshUIState()
{
    // 更新按钮状态
    if (Button_StartCapture && Button_StopCapture && Button_PauseCapture && Button_ResumeCapture)
    {
        bool bIsCapturing = TestActor && TestActor->IsCurrentlyCapturing();
        bool bIsPaused = TestActor && TestActor->IsPaused();
        
        Button_StartCapture->SetIsEnabled(!bIsCapturing);
        Button_StopCapture->SetIsEnabled(bIsCapturing);
        Button_PauseCapture->SetIsEnabled(bIsCapturing && !bIsPaused);
        Button_ResumeCapture->SetIsEnabled(bIsCapturing && bIsPaused);
    }
    
    // 更新状态显示
    UpdateStatusDisplay();
    
    // 重置进度条
    if (ProgressBar_CaptureDuration)
    {
        ProgressBar_CaptureDuration->SetPercent(0.0f);
    }
    
    if (TextBlock_ProgressPercentage)
    {
        TextBlock_ProgressPercentage->SetText(FText::FromString(TEXT("0%")));
    }
}

void UOmniCaptureTestUI::ShowMessage(const FString& Message, bool bIsError)
{
    if (TextBlock_Message)
    {
        TextBlock_Message->SetText(FText::FromString(Message));
        
        // 设置消息颜色
        if (bIsError)
        {
            TextBlock_Message->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
        }
        else
        {
            TextBlock_Message->SetColorAndOpacity(FSlateColor(FLinearColor::Green));
        }
    }
    
    // 重置消息计时器
    MessageTimer = MessageTimeout;
}

void UOmniCaptureTestUI::ClearMessage()
{
    if (TextBlock_Message)
    {
        TextBlock_Message->SetText(FText::GetEmpty());
    }
    
    MessageTimer = 0.0f;
}

void UOmniCaptureTestUI::RegisterButtonEvents()
{
    if (Button_StartCapture)
    {
        Button_StartCapture->OnClicked.AddDynamic(this, &UOmniCaptureTestUI::OnStartCaptureClicked);
    }
    
    if (Button_StopCapture)
    {
        Button_StopCapture->OnClicked.AddDynamic(this, &UOmniCaptureTestUI::OnStopCaptureClicked);
    }
    
    if (Button_PauseCapture)
    {
        Button_PauseCapture->OnClicked.AddDynamic(this, &UOmniCaptureTestUI::OnPauseCaptureClicked);
    }
    
    if (Button_ResumeCapture)
    {
        Button_ResumeCapture->OnClicked.AddDynamic(this, &UOmniCaptureTestUI::OnResumeCaptureClicked);
    }
}

void UOmniCaptureTestUI::RegisterComboBoxEvents()
{
    if (ComboBox_QualityPreset)
    {
        ComboBox_QualityPreset->OnSelectionChanged.AddDynamic(this, &UOmniCaptureTestUI::OnQualityPresetChanged);
    }
}

void UOmniCaptureTestUI::RegisterSpinBoxEvents()
{
    if (SpinBox_ResolutionX)
    {
        SpinBox_ResolutionX->OnValueChanged.AddDynamic(this, &UOmniCaptureTestUI::OnResolutionXChanged);
    }
    
    if (SpinBox_ResolutionY)
    {
        SpinBox_ResolutionY->OnValueChanged.AddDynamic(this, &UOmniCaptureTestUI::OnResolutionYChanged);
    }
    
    if (SpinBox_FrameRate)
    {
        SpinBox_FrameRate->OnValueChanged.AddDynamic(this, &UOmniCaptureTestUI::OnFrameRateChanged);
    }
    
    if (SpinBox_Duration)
    {
        SpinBox_Duration->OnValueChanged.AddDynamic(this, &UOmniCaptureTestUI::OnDurationChanged);
    }
}

void UOmniCaptureTestUI::RegisterCheckBoxEvents()
{
    if (CheckBox_EnableHDR)
    {
        CheckBox_EnableHDR->OnCheckStateChanged.AddDynamic(this, &UOmniCaptureTestUI::OnEnableHDRChanged);
    }
    
    if (CheckBox_ShowStats)
    {
        CheckBox_ShowStats->OnCheckStateChanged.AddDynamic(this, &UOmniCaptureTestUI::OnShowStatsChanged);
    }
}

void UOmniCaptureTestUI::OnStartCaptureClicked()
{
    if (TestActor)
    {
        TestActor->StartCaptureSequence();
        CaptureStartTime = GetWorld()->GetTimeSeconds();
        ShowMessage(TEXT("Capture started"));
    }
    else
    {
        ShowMessage(TEXT("Test actor not set"), true);
    }
}

void UOmniCaptureTestUI::OnStopCaptureClicked()
{
    if (TestActor)
    {
        TestActor->StopCaptureSequence();
        ShowMessage(TEXT("Capture stopped"));
    }
}

void UOmniCaptureTestUI::OnPauseCaptureClicked()
{
    if (TestActor)
    {
        TestActor->PauseCaptureSequence();
        ShowMessage(TEXT("Capture paused"));
    }
}

void UOmniCaptureTestUI::OnResumeCaptureClicked()
{
    if (TestActor)
    {
        TestActor->ResumeCaptureSequence();
        ShowMessage(TEXT("Capture resumed"));
    }
}

void UOmniCaptureTestUI::OnQualityPresetChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (!TestActor) return;
    
    EOmniCaptureQualityPreset Preset = EOmniCaptureQualityPreset::Balanced;
    
    if (SelectedItem == TEXT("Low"))
    {
        Preset = EOmniCaptureQualityPreset::Low;
    }
    else if (SelectedItem == TEXT("Balanced"))
    {
        Preset = EOmniCaptureQualityPreset::Balanced;
    }
    else if (SelectedItem == TEXT("High"))
    {
        Preset = EOmniCaptureQualityPreset::High;
    }
    else if (SelectedItem == TEXT("Ultra"))
    {
        Preset = EOmniCaptureQualityPreset::Ultra;
    }
    else if (SelectedItem == TEXT("Lossless"))
    {
        Preset = EOmniCaptureQualityPreset::Lossless;
    }
    
    TestActor->TestQualityPreset(Preset);
    ShowMessage(FString::Printf(TEXT("Quality preset changed to %s"), *SelectedItem));
}

void UOmniCaptureTestUI::OnResolutionXChanged(float NewValue)
{
    if (TestActor && SpinBox_ResolutionY)
    {
        FIntPoint Resolution = FIntPoint(FMath::RoundToInt(NewValue), FMath::RoundToInt(SpinBox_ResolutionY->GetValue()));
        TestActor->TestResolution(Resolution);
    }
}

void UOmniCaptureTestUI::OnResolutionYChanged(float NewValue)
{
    if (TestActor && SpinBox_ResolutionX)
    {
        FIntPoint Resolution = FIntPoint(FMath::RoundToInt(SpinBox_ResolutionX->GetValue()), FMath::RoundToInt(NewValue));
        TestActor->TestResolution(Resolution);
    }
}

void UOmniCaptureTestUI::OnFrameRateChanged(float NewValue)
{
    if (TestActor)
    {
        TestActor->TestFrameRate(NewValue);
    }
}

void UOmniCaptureTestUI::OnDurationChanged(float NewValue)
{
    if (TestActor)
    {
        TestActor->CaptureDuration = NewValue;
    }
}

void UOmniCaptureTestUI::OnEnableHDRChanged(bool bIsChecked)
{
    if (TestActor)
    {
        TestActor->bEnableHDR = bIsChecked;
        TestActor->SetCaptureHDR(bIsChecked);
        ShowMessage(bIsChecked ? TEXT("HDR capture enabled") : TEXT("HDR capture disabled"));
    }
}

void UOmniCaptureTestUI::OnShowStatsChanged(bool bIsChecked)
{
    if (TestActor)
    {
        TestActor->bShowEncodingStats = bIsChecked;
    }
}

void UOmniCaptureTestUI::UpdateStatusDisplay()
{
    if (!TextBlock_CaptureStatus || !TestActor) return;
    
    FString StatusText = TEXT("Status: Idle");
    FLinearColor StatusColor = FLinearColor::Gray;
    
    if (TestActor->IsCurrentlyCapturing())
    {
        if (TestActor->IsPaused())
        {
            StatusText = TEXT("Status: Capturing (Paused)");
            StatusColor = FLinearColor::Yellow;
        }
        else
        {
            StatusText = TEXT("Status: Capturing");
            StatusColor = FLinearColor::Green;
        }
    }
    
    TextBlock_CaptureStatus->SetText(FText::FromString(StatusText));
    TextBlock_CaptureStatus->SetColorAndOpacity(FSlateColor(StatusColor));
}

void UOmniCaptureTestUI::UpdateProgressDisplay(float DeltaTime)
{
    if (!ProgressBar_CaptureDuration || !TextBlock_ProgressPercentage || !TestActor) return;
    
    if (TestActor->IsCurrentlyCapturing() && TestActor->CaptureDuration > 0.0f)
    {
        float CurrentTime = GetWorld()->GetTimeSeconds();
        float ElapsedTime = CurrentTime - CaptureStartTime;
        float Progress = FMath::Clamp(ElapsedTime / TestActor->CaptureDuration, 0.0f, 1.0f);
        
        ProgressBar_CaptureDuration->SetPercent(Progress);
        
        FString PercentageText = FString::Printf(TEXT("%.0f%%"), Progress * 100.0f);
        TextBlock_ProgressPercentage->SetText(FText::FromString(PercentageText));
    }
    else
    {
        ProgressBar_CaptureDuration->SetPercent(0.0f);
        TextBlock_ProgressPercentage->SetText(FText::FromString(TEXT("0%")));
    }
}

void UOmniCaptureTestUI::UpdateStatsDisplay()
{
    if (!TextBlock_EncodingStats || !TestActor) return;
    
    float AverageBitrate = 0.0f;
    float CurrentFPS = 0.0f;
    TestActor->GetEncodingStatistics(AverageBitrate, CurrentFPS);
    
    FString StatsText = FString::Printf(TEXT("Bitrate: %.2f Mbps | FPS: %.1f"), AverageBitrate, CurrentFPS);
    TextBlock_EncodingStats->SetText(FText::FromString(StatsText));
}

void UOmniCaptureTestUI::BindTestActorEvents()
{
    if (TestActor)
    {
        // 绑定事件代理
        TestActor->OnCaptureCompleted.AddDynamic(this, &UOmniCaptureTestUI::OnCaptureCompleted);
        TestActor->OnCaptureError.AddDynamic(this, &UOmniCaptureTestUI::OnCaptureError);
        TestActor->OnEncodingStatisticsUpdated.AddDynamic(this, &UOmniCaptureTestUI::OnEncodingStatisticsUpdated);
    }
}

void UOmniCaptureTestUI::OnCaptureCompleted(const FString& OutputFilePath)
{
    FString Message = FString::Printf(TEXT("Capture completed. File: %s"), *OutputFilePath);
    ShowMessage(Message);
    
    // 更新输出文件路径显示
    if (TextBlock_OutputFilePath)
    {
        TextBlock_OutputFilePath->SetText(FText::FromString(OutputFilePath));
    }
}

void UOmniCaptureTestUI::OnCaptureError(const FString& ErrorMessage)
{
    ShowMessage(ErrorMessage, true);
}

void UOmniCaptureTestUI::OnEncodingStatisticsUpdated(float AverageBitrate, float CurrentFPS)
{
    // 这个方法主要用于接收来自测试Actor的统计信息更新
    // 实际显示在UpdateStatsDisplay方法中处理
    
    // 这里可以添加额外的处理逻辑
    if (CheckBox_ShowStats && CheckBox_ShowStats->IsChecked())
    {
        UpdateStatsDisplay();
    }
}
