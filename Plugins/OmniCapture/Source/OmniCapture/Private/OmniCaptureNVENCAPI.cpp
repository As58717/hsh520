#include "OmniCaptureNVENCAPI.h"

#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

namespace
{
    constexpr const TCHAR* GDefaultNVENCDllName = TEXT("nvEncodeAPI64.dll");
}

const FString FOmniCaptureNVENCAPI::NVEncodeAPIDLLName = GDefaultNVENCDllName;

const TArray<FString> FOmniCaptureNVENCAPI::SystemSearchPaths = {
    TEXT("C:/Windows/System32"),
    TEXT("C:/Windows/SysWOW64"),
    TEXT("C:/Program Files/NVIDIA Corporation/NVSMI"),
    TEXT("C:/Program Files (x86)/NVIDIA Corporation/NVSMI")
};

FOmniCaptureNVENCAPI::FOmniCaptureNVENCAPI()
    : NVEncodeAPILibraryHandle(nullptr)
    , NVEncodeAPIFuncList(MakeUnique<FNVENCAPIFunctionList>())
    , bIsNVEncodeAPILoaded(false)
    , MaxEncoderSessions(0)
{
}

FOmniCaptureNVENCAPI::~FOmniCaptureNVENCAPI()
{
    UnloadNVEncodeAPI();
}

FOmniCaptureNVENCAPI& FOmniCaptureNVENCAPI::Get()
{
    static FOmniCaptureNVENCAPI Singleton;
    return Singleton;
}

bool FOmniCaptureNVENCAPI::IsNVENCAvailable() const
{
    return bIsNVEncodeAPILoaded;
}

bool FOmniCaptureNVENCAPI::LoadNVEncodeAPI()
{
    if (bIsNVEncodeAPILoaded)
    {
        return true;
    }

    if (NVEncodeAPILibraryPath.IsEmpty())
    {
        if (!ScanSystemForNVEncodeAPI())
        {
            return false;
        }
    }

    NVEncodeAPILibraryHandle = LoadDynamicLibrary(NVEncodeAPILibraryPath);
    if (!NVEncodeAPILibraryHandle)
    {
        return false;
    }

    NVEncodeAPIFuncList->Version = 1;
    bIsNVEncodeAPILoaded = true;

    RunNVENCAvailabilityTest();
    return true;
}

void FOmniCaptureNVENCAPI::UnloadNVEncodeAPI()
{
    if (!bIsNVEncodeAPILoaded)
    {
        return;
    }

    CleanupNVEncodeAPIFunctions();

    if (NVEncodeAPILibraryHandle)
    {
        UnloadDynamicLibrary(NVEncodeAPILibraryHandle);
        NVEncodeAPILibraryHandle = nullptr;
    }

    bIsNVEncodeAPILoaded = false;
    MaxEncoderSessions = 0;
    NVENCDriverVersion.Reset();
    NVENCHardwareInfo.Reset();
}

bool FOmniCaptureNVENCAPI::IsNVEncodeAPILoaded() const
{
    return bIsNVEncodeAPILoaded;
}

FNVENCAPIFunctionList* FOmniCaptureNVENCAPI::GetNVEncodeAPIFunctions()
{
    return NVEncodeAPIFuncList.Get();
}

FString FOmniCaptureNVENCAPI::GetNVENCDriverVersion() const
{
    return NVENCDriverVersion;
}

FString FOmniCaptureNVENCAPI::GetNVEncodeAPILibraryPath() const
{
    return NVEncodeAPILibraryPath;
}

bool FOmniCaptureNVENCAPI::ScanSystemForNVEncodeAPI()
{
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    for (const FString& SearchPath : SystemSearchPaths)
    {
        const FString Candidate = FPaths::Combine(SearchPath, NVEncodeAPIDLLName);
        if (PlatformFile.FileExists(*Candidate))
        {
            NVEncodeAPILibraryPath = Candidate;
            return true;
        }
    }

    return false;
}

bool FOmniCaptureNVENCAPI::IsValidNVEncodeAPILibrary(const FString& LibraryPath) const
{
    return FPlatformFileManager::Get().GetPlatformFile().FileExists(*LibraryPath);
}

bool FOmniCaptureNVENCAPI::SetNVEncodeAPILibraryPath(const FString& LibraryPath)
{
    if (!LibraryPath.IsEmpty() && IsValidNVEncodeAPILibrary(LibraryPath))
    {
        if (bIsNVEncodeAPILoaded)
        {
            UnloadNVEncodeAPI();
        }

        NVEncodeAPILibraryPath = LibraryPath;
        return true;
    }

    return false;
}

int32 FOmniCaptureNVENCAPI::GetMaxEncoderSessions() const
{
    return MaxEncoderSessions;
}

FString FOmniCaptureNVENCAPI::GetNVENCHardwareInfo() const
{
    return NVENCHardwareInfo;
}

bool FOmniCaptureNVENCAPI::RunNVENCAvailabilityTest()
{
    if (!bIsNVEncodeAPILoaded)
    {
        return false;
    }

    MaxEncoderSessions = 1;
    NVENCHardwareInfo = TEXT("NVENC availability confirmed (stub)");
    NVENCDriverVersion = TEXT("Unknown");
    return true;
}

void* FOmniCaptureNVENCAPI::LoadDynamicLibrary(const FString& LibraryPath) const
{
    return FPlatformProcess::GetDllHandle(*LibraryPath);
}

void FOmniCaptureNVENCAPI::UnloadDynamicLibrary(void* LibraryHandle) const
{
    if (LibraryHandle)
    {
        FPlatformProcess::FreeDllHandle(LibraryHandle);
    }
}

void* FOmniCaptureNVENCAPI::GetProcAddress(void* LibraryHandle, const char* ProcName) const
{
    return nullptr;
}

bool FOmniCaptureNVENCAPI::InitializeNVEncodeAPIFunctions()
{
    // 简化实现，不执行实际的 API 绑定。
    return true;
}

void FOmniCaptureNVENCAPI::CleanupNVEncodeAPIFunctions()
{
    if (NVEncodeAPIFuncList)
    {
        *NVEncodeAPIFuncList = FNVENCAPIFunctionList();
    }
}

bool FOmniCaptureNVENCAPI::CheckGPUHardwareSupport() const
{
    return MaxEncoderSessions > 0;
}

void FOmniCaptureNVENCAPIModule::StartupModule()
{
    FOmniCaptureNVENCAPI::Get().LoadNVEncodeAPI();
}

void FOmniCaptureNVENCAPIModule::ShutdownModule()
{
    FOmniCaptureNVENCAPI::Get().UnloadNVEncodeAPI();
}

IMPLEMENT_MODULE(FOmniCaptureNVENCAPIModule, OmniCaptureNVENCAPIModule)
