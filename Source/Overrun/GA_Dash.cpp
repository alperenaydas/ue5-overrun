// Fill out your copyright notice in the Description page of Project Settings.


#include "GA_Dash.h"

#include "OverrunGameplayTags.h"
#include "TopDownCMC.h"

UGA_Dash::UGA_Dash()
{
	const FGameplayTagContainer AbilityTagsToSet(TAG_Ability_Dash);
	SetAssetTags(AbilityTagsToSet);
}

void UGA_Dash::ActivateMovementAbility(UTopDownCMC* CMC)
{
	CMC->TriggerDashing();
}
