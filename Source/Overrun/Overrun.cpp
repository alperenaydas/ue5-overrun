// Copyright Epic Games, Inc. All Rights Reserved.

#include "Overrun.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_PRIMARY_GAME_MODULE( FDefaultGameModuleImpl, Overrun, "Overrun" );

static TAutoConsoleVariable<int32> CVarDebugHealth(
	TEXT("Overrun.Debug.Health"),
	0,
	TEXT("Debug health displays: 0 = hidden, 1 = visible."),
	ECVF_Default);