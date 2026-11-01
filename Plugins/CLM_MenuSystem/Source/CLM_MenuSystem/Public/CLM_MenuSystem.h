// Copyright Vedant Vats.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FCLM_MenuSystemModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void OnPostEngineInit();
	FDelegateHandle PostEngineInitHandle;
};
