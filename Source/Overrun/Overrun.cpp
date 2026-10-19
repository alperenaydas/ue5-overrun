// Copyright Epic Games, Inc. All Rights Reserved.

#include "Overrun.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_PRIMARY_GAME_MODULE( FDefaultGameModuleImpl, Overrun, "Overrun" );

static TAutoConsoleVariable<int32> CVarDebugHealth(
	TEXT("Overrun.Debug.Health"),
	0,
	TEXT("Debug health displays: 0 = hidden, 1 = visible."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarDebugCapsules(
	TEXT("Overrun.Debug.Capsules"),
	0,
	TEXT("Mass hit capsules: 0 = hidden, 1 = visible."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarDebugShotTraces(
	TEXT("Overrun.Debug.ShotTraces"),
	0,
	TEXT("Local shot traces: 0 = hidden, 1 = visible."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarDebugHUD(
	TEXT("Overrun.Debug.HUD"),
	0,
	TEXT("Debug HUD: 0 = hidden, 1 = visible."),
	ECVF_Default);