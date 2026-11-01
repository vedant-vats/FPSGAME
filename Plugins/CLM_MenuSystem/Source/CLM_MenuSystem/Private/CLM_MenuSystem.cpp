// Copyright Vedant Vats.

#include "CLM_MenuSystem.h"
#include "CLMUserSettingsSubsystem.h"
#include "Misc/CoreDelegates.h"
#include "Subsystems/SubsystemCollection.h"

#define LOCTEXT_NAMESPACE "FCLM_MenuSystemModule"

namespace CLMMenu
{
	// The Blueprint child that holds the actual save/load logic.
	static const TCHAR* SettingsSubsystemBPPath =
		TEXT("/CLM_MenuSystem/Blueprints/GameMode/BPCLMUserSettingsSubSystem.BPCLMUserSettingsSubSystem_C");
}

void FCLM_MenuSystemModule::StartupModule()
{
	PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddRaw(this, &FCLM_MenuSystemModule::OnPostEngineInit);
}

void FCLM_MenuSystemModule::OnPostEngineInit()
{
	// Blueprint subsystems only get created if their class is loaded.
	// Load it here, then activate it for any game instance that already exists (packaged builds).
	if (UClass* SettingsClass = LoadClass<UCLMUserSettingsSubsystem>(nullptr, CLMMenu::SettingsSubsystemBPPath))
	{
		SettingsClass->AddToRoot();
		FSubsystemCollectionBase::ActivateExternalSubsystem(SettingsClass);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("CLM_MenuSystem: could not load %s"), CLMMenu::SettingsSubsystemBPPath);
	}
}

void FCLM_MenuSystemModule::ShutdownModule()
{
	FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCLM_MenuSystemModule, CLM_MenuSystem)
