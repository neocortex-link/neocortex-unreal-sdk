#include "NeocortexMicrophonePermission.h"

#if PLATFORM_ANDROID
#include "AndroidPermissionFunctionLibrary.h"
#endif

bool UNeocortexMicrophonePermission::HasMicrophonePermission()
{
#if PLATFORM_ANDROID
    return UAndroidPermissionFunctionLibrary::CheckPermission(TEXT("android.permission.RECORD_AUDIO"));
#else
    // iOS: controlled by NSMicrophoneUsageDescription (OS prompts automatically). Desktop: always available.
    return true;
#endif
}

void UNeocortexMicrophonePermission::RequestMicrophonePermission()
{
#if PLATFORM_ANDROID
    TArray<FString> Permissions;
    Permissions.Add(TEXT("android.permission.RECORD_AUDIO"));
    UAndroidPermissionFunctionLibrary::AcquirePermissions(Permissions);
#endif
}
