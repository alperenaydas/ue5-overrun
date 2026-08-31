// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "OverrunAttributeSet.generated.h"

/**
 * 
 */
UCLASS()
class OVERRUN_API UOverrunAttributeSet : public UAttributeSet
{
	GENERATED_BODY()
	
public:
	UPROPERTY(ReplicatedUsing=OnRep_Stamina)
	FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS_BASIC(UOverrunAttributeSet, Stamina)
	
public:
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
private:
	UFUNCTION()
	void OnRep_Stamina(const FGameplayAttributeData& OldStamina) const;
};
