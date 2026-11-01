// Copyright Vedant Vats.

#include "CLMUserSettingsSubsystem.h"
#include "UObject/UObjectHash.h"

bool UCLMUserSettingsSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Only create the most-derived class, so we never get two settings subsystems.
	TArray<UClass*> ChildClasses;
	GetDerivedClasses(GetClass(), ChildClasses, false);
	return ChildClasses.Num() == 0;
}
