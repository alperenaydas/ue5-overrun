// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "TopDownPlayerState.generated.h"

struct FOnAttributeChangeData;
class UOverrunAttributeSet;
class UAbilitySystemComponent;
/**
 * 
 */
UCLASS()
class OVERRUN_API ATopDownPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()
	
public:
	ATopDownPlayerState();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	void SetDeadState(bool isDead);
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
protected:
	virtual void BeginPlay() override;
	
public:
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	
	UPROPERTY()
	TObjectPtr<UOverrunAttributeSet> OverrunAttributeSet;
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_IsDead)
	bool IsDead = false;
	
private:
	UFUNCTION()
	void OnRep_IsDead(bool bOldIsDead) const;
	
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void CharacterEnterDeadTransition() const;
};
