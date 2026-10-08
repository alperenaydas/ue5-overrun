// Fill out your copyright notice in the Description page of Project Settings.


#include "Overrun/Debug/OverrunDebugHealthComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "Overrun/AbilitySystem/OverrunAttributeSet.h"
#include "Overrun/Debug/OverrunDebugHealthWidget.h"

UOverrunDebugHealthComponent::UOverrunDebugHealthComponent()
{
	SetWidgetSpace(EWidgetSpace::Screen);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HealthAttribute = UOverrunAttributeSet::GetHealthAttribute();
	MaxHealthAttribute = UOverrunAttributeSet::GetMaxHealthAttribute();
}

void UOverrunDebugHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld(); World && World->GetNetMode() != NM_DedicatedServer)
	{
		// Actor/PlayerState initialization can happen after component BeginPlay.
		// Keep checking identity so later ASC replacements are handled as well.
		World->GetTimerManager().SetTimer(AbilitySystemConnectionTimerHandle, this,
		                                  &UOverrunDebugHealthComponent::RefreshAbilitySystemConnection, 0.25f, true);
	}
}

void UOverrunDebugHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AbilitySystemConnectionTimerHandle);
	}
	if (UOverrunDebugHealthWidget* HealthWidget = Cast<UOverrunDebugHealthWidget>(GetUserWidgetObject()))
	{
		HealthWidget->InitializeFromASC(nullptr, HealthAttribute, MaxHealthAttribute);
	}
	Super::EndPlay(EndPlayReason);
}

void UOverrunDebugHealthComponent::InitWidget()
{
	Super::InitWidget();
	RefreshAbilitySystemConnection();
}

void UOverrunDebugHealthComponent::RefreshAbilitySystemConnection()
{
	if (GetWorld() && GetWorld()->IsGameWorld())
	{
		if (UOverrunDebugHealthWidget* HealthWidget = Cast<UOverrunDebugHealthWidget>(GetUserWidgetObject()))
		{
			UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
			HealthWidget->InitializeFromASC(ASC, HealthAttribute, MaxHealthAttribute);
		}
	}
}
