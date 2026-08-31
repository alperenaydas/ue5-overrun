// Fill out your copyright notice in the Description page of Project Settings.


#include "GA_Sprint.h"

#include "OverrunGameplayTags.h"
#include "../Movement/TopDownCMC.h"

UGA_Sprint::UGA_Sprint()
{
	const FGameplayTagContainer AbilityTagsToSet(TAG_Ability_Sprint);
	SetAssetTags(AbilityTagsToSet);
}

void UGA_Sprint::ActivateMovementAbility(UTopDownCMC* CMC)
{
	CMC->SetSprinting(true);
}
