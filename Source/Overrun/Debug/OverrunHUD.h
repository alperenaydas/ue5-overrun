// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "OverrunHUD.generated.h"

/**
 * 
 */
UCLASS()
class OVERRUN_API AOverrunHUD : public AHUD
{
	GENERATED_BODY()
	
public:
	UFUNCTION()
	virtual void DrawHUD() override;
};
