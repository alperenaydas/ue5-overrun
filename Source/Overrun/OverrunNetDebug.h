// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"

#if !UE_BUILD_SHIPPING

inline FString GetRoleName(const ENetRole Role)
{
	FString RoleText;
	switch (Role)
	{
	case ROLE_Authority:       RoleText = TEXT("AUTHORITY (server)"); break;
	case ROLE_AutonomousProxy: RoleText = TEXT("AUTONOMOUS");         break;
	case ROLE_SimulatedProxy:  RoleText = TEXT("SIMULATED");          break;
	default:                   RoleText = TEXT("NONE");               break;
	}
	return RoleText;
}

inline FString GetActivationModeName(const EGameplayAbilityActivationMode::Type ActivationMode)
{
	FString ActivationModeText;
	switch (ActivationMode)
	{
	case EGameplayAbilityActivationMode::Authority:      ActivationModeText = TEXT("AUTHORITY (server)"); break;
	case EGameplayAbilityActivationMode::NonAuthority:   ActivationModeText = TEXT("NON-AUTHORITY");      break;
	case EGameplayAbilityActivationMode::Predicting:     ActivationModeText = TEXT("PREDICTING");         break;
	case EGameplayAbilityActivationMode::Confirmed:      ActivationModeText = TEXT("CONFIRMED");          break;
	case EGameplayAbilityActivationMode::Rejected:       ActivationModeText = TEXT("REJECTED");           break;
	default:                                             ActivationModeText = TEXT("UNKNOWN");            break;
	}
	return ActivationModeText;
}

#endif