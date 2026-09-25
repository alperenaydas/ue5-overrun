// Fill out your copyright notice in the Description page of Project Settings.


#include "Overrun/Test/TestCube.h"
#include "AbilitySystemComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Overrun/AbilitySystem/OverrunAttributeSet.h"
#include "Overrun/AbilitySystem/OverrunGameplayTags.h"

ATestCube::ATestCube()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SetRootComponent(Mesh);
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("ASC"));
	AttributeSet = CreateDefaultSubobject<UOverrunAttributeSet>(TEXT("AttributeSet"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	bReplicates = true;
}

void ATestCube::BeginPlay()
{
	Super::BeginPlay();
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
		if (HasAuthority())
		{
			AbilitySystemComponent->SetNumericAttributeBase(UOverrunAttributeSet::GetMaxHealthAttribute(), DefaultMaxHealth);
			AbilitySystemComponent->SetNumericAttributeBase(UOverrunAttributeSet::GetHealthAttribute(), DefaultMaxHealth);
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHealthAttribute()).AddUObject(this, &ATestCube::HandleHealthChanged);
		}
	}
}

UAbilitySystemComponent* ATestCube::GetAbilitySystemComponent() const
{
	if (AbilitySystemComponent)
	{
		return AbilitySystemComponent;
	}
	return nullptr;
}

void ATestCube::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	if (HasAuthority())
	{

		if (Data.OldValue > 0 && Data.NewValue <= 0)
		{
			// death
			AbilitySystemComponent->SetLooseGameplayTagCount(TAG_State_Dead, 1);
			Destroy();
		}
	}
}

