// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "Components/WidgetComponent.h"
#include "TimerManager.h"
#include "OverrunDebugHealthComponent.generated.h"

UCLASS(ClassGroup = (Debug), meta = (BlueprintSpawnableComponent))
class OVERRUN_API UOverrunDebugHealthComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UOverrunDebugHealthComponent();
	virtual void InitWidget() override;

	void RefreshAbilitySystemConnection();

	UPROPERTY(EditAnywhere, Category = "Debug Health")
	FGameplayAttribute HealthAttribute;

	UPROPERTY(EditAnywhere, Category = "Debug Health")
	FGameplayAttribute MaxHealthAttribute;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	FTimerHandle AbilitySystemConnectionTimerHandle;
};
