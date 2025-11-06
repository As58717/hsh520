#include "OmniCaptureEditorModule.h"

#include "LevelEditor.h"
#include "OmniCaptureEditorSettings.h"
#include "SOmniCaptureControlPanel.h"
#include "OmniCaptureNVENCStatus.h"
#include "OmniCaptureNVENCAPI.h"
#include "Framework/Docking/TabManager.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Misc/EngineVersionComparison.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Editor.h"
#include "CoreDelegates.h"

static const FName OmniCapturePanelTabName(TEXT("OmniCapturePanel"));
static const FName OmniCaptureNVENCTabName(TEXT("OmniCaptureNVENC"));

#define LOCTEXT_NAMESPACE "FOmniCaptureEditorModule"

void FOmniCaptureEditorModule::StartupModule()
{
    // 注册主要捕获面板TabSpawner
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(OmniCapturePanelTabName, FOnSpawnTab::CreateRaw(this, &FOmniCaptureEditorModule::SpawnCaptureTab))
        .SetDisplayName(NSLOCTEXT("OmniCaptureEditor", "CapturePanelTitle", "Omni Capture"))
        .SetTooltipText(NSLOCTEXT("OmniCaptureEditor", "CapturePanelTooltip", "Open the Omni Capture control panel"))
        .SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
    
    // 注册NVENC监控TabSpawner
    RegisterNVENCTabSpawner();

#if UE_VERSION_OLDER_THAN(5, 6, 0)
    MenuRegistrationHandle = UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FOmniCaptureEditorModule::RegisterMenus));
#else
    MenuRegistrationHandle = UToolMenus::Get()->RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FOmniCaptureEditorModule::RegisterMenus));
#endif

    if (const UOmniCaptureEditorSettings* Settings = GetDefault<UOmniCaptureEditorSettings>())
    {
        if (Settings->bAutoOpenPanel)
        {
            FGlobalTabmanager::Get()->TryInvokeTab(OmniCapturePanelTabName);
        }
    }
    
    // 初始化NVENC API
    InitNVENCSupport();
    
    // 注册编辑器模块的退出回调
    FCoreDelegates::OnFEngineLoopDestroyed.AddRaw(this, &FOmniCaptureEditorModule::OnEditorClose);
}

void FOmniCaptureEditorModule::ShutdownModule()
{
    if (UToolMenus* ToolMenus = UToolMenus::TryGet())
    {
        if (MenuRegistrationHandle.IsValid())
        {
#if UE_VERSION_OLDER_THAN(5, 6, 0)
            UToolMenus::UnregisterStartupCallback(MenuRegistrationHandle);
#endif
        }
        ToolMenus->UnregisterOwner(this);
    }

    MenuRegistrationHandle = FDelegateHandle();

    // 注销TabSpawners
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(OmniCapturePanelTabName);
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(OmniCaptureNVENCTabName);
    
    // 卸载NVENC API
    FOmniCaptureNVENCAPI::Get().UnloadNVEncodeAPI();
    
    // 移除委托
    FCoreDelegates::OnFEngineLoopDestroyed.RemoveAll(this);
}

TSharedRef<SDockTab> FOmniCaptureEditorModule::SpawnCaptureTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SOmniCaptureControlPanel)
        ];
}

void FOmniCaptureEditorModule::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);
    
    // 添加OmniCapture主面板到工具栏
    if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar"))
    {
        FToolMenuSection& Section = Menu->FindOrAddSection("OmniCapture");
        Section.AddEntry(FToolMenuEntry::InitToolBarButton(
            TEXT("OmniCaptureToggle"),
            FUIAction(FExecuteAction::CreateRaw(this, &FOmniCaptureEditorModule::HandleOpenPanel)),
            NSLOCTEXT("OmniCaptureEditor", "ToolbarLabel", "Omni Capture"),
            NSLOCTEXT("OmniCaptureEditor", "ToolbarTooltip", "Open the Omni Capture control panel"),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details")));
        
        // 添加NVENC监控面板到工具栏
        Section.AddEntry(FToolMenuEntry::InitToolBarButton(
            TEXT("OmniCaptureNVENCToggle"),
            FUIAction(FExecuteAction::CreateRaw(this, &FOmniCaptureEditorModule::HandleOpenNVENCPanel)),
            NSLOCTEXT("OmniCaptureEditor", "NVENCToolbarLabel", "NVENC Monitor"),
            NSLOCTEXT("OmniCaptureEditor", "NVENCToolbarTooltip", "Open the NVENC monitoring panel"),
            FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Settings")));
    }
    
    // 在Window菜单中添加NVENC监控菜单项
    if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window"))
    {
        FToolMenuSection& Section = Menu->FindOrAddSection("WorkspaceTools");
        Section.AddMenuEntryWithCommandList(
            OmniCaptureNVENCTabName,
            NSLOCTEXT("OmniCaptureEditor", "NVENCMenuLabel", "OmniCapture NVENC Monitor"),
            NSLOCTEXT("OmniCaptureEditor", "NVENCMenuTooltip", "Open NVENC monitoring panel"),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateRaw(this, &FOmniCaptureEditorModule::HandleOpenNVENCPanel))
        );
    }
}

void FOmniCaptureEditorModule::HandleOpenPanel()
{
    FGlobalTabmanager::Get()->TryInvokeTab(OmniCapturePanelTabName);
}

void FOmniCaptureEditorModule::RegisterNVENCTabSpawner()
{
    // 获取工作区菜单结构
    IWorkspaceMenuStructure& WorkspaceMenuStructure = WorkspaceMenu::GetMenuStructure();
    
    // 注册NVENC监控TabSpawner
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(OmniCaptureNVENCTabName, FOnSpawnTab::CreateRaw(this, &FOmniCaptureEditorModule::SpawnNVENCTab))
        .SetDisplayName(NSLOCTEXT("OmniCaptureEditor", "NVENCPanelTitle", "NVENC Monitor"))
        .SetTooltipText(NSLOCTEXT("OmniCaptureEditor", "NVENCPanelTooltip", "Monitor NVENC availability and settings"))
        .SetGroup(WorkspaceMenuStructure.GetToolsCategory())
        .SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Settings"));
}

TSharedRef<SDockTab> FOmniCaptureEditorModule::SpawnNVENCTab(const FSpawnTabArgs& SpawnTabArgs)
{
    TSharedRef<SDockTab> DockTab = SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .OnTabClosed_Lambda([this](TSharedRef<SDockTab> Tab) {
            UE_LOG(LogTemp, Log, TEXT("NVENC Monitor tab closed"));
        });
    
    DockTab->SetContent(
        SNew(SBox)
        .Padding(10.0f)
        [
            SNew(SVerticalBox)
            
            // 标题
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 10.0f)
            [
                SNew(STextBlock)
                .Text(NSLOCTEXT("OmniCaptureEditor", "NVENCStatusTitle", "NVENC Status Monitor"))
                .Font(FAppStyle::GetFontStyle("LargeFont"))
                .ColorAndOpacity(FSlateColor::UseForeground())
            ]
            
            // NVENC状态监控组件
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 10.0f)
            [
                SAssignNew(NVENCStatusWidget, SOmniCaptureNVENCStatus)
                .bAutoRefresh(true)
                .RefreshInterval(30.0f) // 30秒自动刷新一次
                .OnStatusChanged(this, &FOmniCaptureEditorModule::HandleNVENCStatusChanged)
            ]
            
            // 说明文本
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(STextBlock)
                .Text(NSLOCTEXT("OmniCaptureEditor", "NVENCDescription", "Use this panel to monitor NVENC availability and test encoding functionality.\n" 
                "For best results, ensure you have the latest NVIDIA drivers installed."))
                .Font(FAppStyle::GetFontStyle("NormalFont"))
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                .AutoWrapText(true)
            ]
        ]
    );
    
    return DockTab;
}

void FOmniCaptureEditorModule::HandleOpenNVENCPanel()
{
    FGlobalTabmanager::Get()->TryInvokeTab(OmniCaptureNVENCTabName);
}

void FOmniCaptureEditorModule::InitNVENCSupport()
{
    // 获取NVENC API管理器
    FOmniCaptureNVENCAPI& NVENCAPI = FOmniCaptureNVENCAPI::Get();
    
    // 扫描系统中的NVENC库
    bool bFound = NVENCAPI.ScanSystemForNVEncodeAPI();
    
    // 尝试加载NVENC API
    if (bFound)
    {
        bool bLoaded = NVENCAPI.LoadNVEncodeAPI();
        
        // 运行可用性测试
        if (bLoaded)
        {
            NVENCAPI.RunNVENCAvailabilityTest();
        }
    }
}

void FOmniCaptureEditorModule::HandleNVENCStatusChanged(bool bAvailable)
{
    // 处理NVENC状态变更
    if (bAvailable)
    {
        // 可以在这里添加成功通知逻辑
    }
    else
    {
        // 可以在这里添加警告通知逻辑
    }
}

void FOmniCaptureEditorModule::OnEditorClose()
{
    // 编辑器关闭时的清理工作
    FOmniCaptureNVENCAPI::Get().UnloadNVEncodeAPI();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FOmniCaptureEditorModule, OmniCaptureEditor)
