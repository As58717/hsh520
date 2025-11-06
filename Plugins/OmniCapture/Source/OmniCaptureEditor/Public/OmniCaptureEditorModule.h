
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Delegates/Delegate.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

class FSpawnTabArgs;
class SDockTab;
class SOmniCaptureNVENCStatus;

class FOmniCaptureEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    // 主要捕获面板相关
    TSharedRef<SDockTab> SpawnCaptureTab(const FSpawnTabArgs& Args);
    void RegisterMenus();
    void HandleOpenPanel();
    
    // NVENC监控相关
    void RegisterNVENCTabSpawner();
    TSharedRef<SDockTab> SpawnNVENCTab(const FSpawnTabArgs& SpawnTabArgs);
    void HandleOpenNVENCPanel();
    void InitNVENCSupport();
    void HandleNVENCStatusChanged(bool bAvailable);
    void OnEditorClose();

private:
    FDelegateHandle MenuRegistrationHandle;
    TSharedPtr<SOmniCaptureNVENCStatus> NVENCStatusWidget;
};
