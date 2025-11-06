#pragma once

#include "CoreMinimal.h"
#include "HAL/RunnableThread.h"

#include "OmniCaptureNVENCConfig.generated.h"

/**
 * Configuration for the OmniCapture NVENC encoder.
 *
 * The engine's thread priority enum is not reflected, so we expose our own
 * Blueprint-safe wrapper and convert it back to the engine value when needed.
 */
UENUM(BlueprintType)
enum class EOmniCaptureThreadPriority : uint8
{
    Lowest      UMETA(DisplayName = "Lowest"),
    BelowNormal UMETA(DisplayName = "Below Normal"),
    Normal      UMETA(DisplayName = "Normal"),
    AboveNormal UMETA(DisplayName = "Above Normal"),
    Highest     UMETA(DisplayName = "Highest"),
};

inline EThreadPriority ToThreadPriority(EOmniCaptureThreadPriority InPriority)
{
    switch (InPriority)
    {
        case EOmniCaptureThreadPriority::Lowest:
            return EThreadPriority::TPri_Lowest;
        case EOmniCaptureThreadPriority::BelowNormal:
            return EThreadPriority::TPri_BelowNormal;
        case EOmniCaptureThreadPriority::Normal:
            return EThreadPriority::TPri_Normal;
        case EOmniCaptureThreadPriority::AboveNormal:
            return EThreadPriority::TPri_AboveNormal;
        case EOmniCaptureThreadPriority::Highest:
        default:
            return EThreadPriority::TPri_TimeCritical;
    }
}

inline EOmniCaptureThreadPriority FromThreadPriority(EThreadPriority InPriority)
{
    switch (InPriority)
    {
        case EThreadPriority::TPri_Lowest:
            return EOmniCaptureThreadPriority::Lowest;
        case EThreadPriority::TPri_BelowNormal:
            return EOmniCaptureThreadPriority::BelowNormal;
        case EThreadPriority::TPri_AboveNormal:
            return EOmniCaptureThreadPriority::AboveNormal;
        case EThreadPriority::TPri_TimeCritical:
            return EOmniCaptureThreadPriority::Highest;
        case EThreadPriority::TPri_Normal:
        default:
            return EOmniCaptureThreadPriority::Normal;
    }
}

USTRUCT(BlueprintType)
struct FOmniCaptureNVENCConfig
{
    GENERATED_BODY()

    FOmniCaptureNVENCConfig()
        : ThreadPriority(EOmniCaptureThreadPriority::Normal)
    {
    }

    /** Priority that should be used for the encoder worker thread. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OmniCapture|NVENC")
    EOmniCaptureThreadPriority ThreadPriority;
};

