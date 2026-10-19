// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "Blueprint/UserWidget.h"
#include "HAL/IConsoleManager.h"
#include "OverrunDebugHealthWidget.generated.h"

struct FOnAttributeChangeData;
class UAbilitySystemComponent;
class UTextBlock;

UCLASS()
class OVERRUN_API UOverrunDebugHealthWidget : public UUserWidget
{
	GENERATED_BODY()

	void RefreshDebugVisibility();
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	void RefreshHealthText();
	void HealthChanged(const FOnAttributeChangeData& Data);
	void BindHealthDelegates();
	void UnbindHealthDelegates();

	FConsoleVariableSinkHandle ConsoleVariableSinkHandle;
	bool bConsoleSinkRegistered = false;

	FDelegateHandle HealthChangedDelegateHandle;
	FDelegateHandle MaxHealthChangedDelegateHandle;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HealthText;

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(Transient)
	FGameplayAttribute HealthAttribute;

	UPROPERTY(Transient)
	FGameplayAttribute MaxHealthAttribute;

public:
	void InitializeFromASC(UAbilitySystemComponent* InASC, const FGameplayAttribute& InHealthAttribute,
	                       const FGameplayAttribute& InMaxHealthAttribute);
	void SetHealthValues(float InHealth, float InMaxHealth);
};
