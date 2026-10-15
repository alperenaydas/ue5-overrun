// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OverrunMassReplicationSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class OVERRUN_API UOverrunMassReplicationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void PostInitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
};
