// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

NEOCORTEX_API DECLARE_LOG_CATEGORY_EXTERN(LogNeocortex, Log, All);

class NEOCORTEX_API FNeocortexModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
