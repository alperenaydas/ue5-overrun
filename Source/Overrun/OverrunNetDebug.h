// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

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

#endif
