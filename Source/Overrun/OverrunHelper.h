#pragma once


#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "DrawDebugHelpers.h"

namespace OverrunHelper
{
	inline void TryActivateAbilityByTag(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayTag Tag)
	{
		if (AbilitySystemComponent)
		{
			const FGameplayTagContainer AbilityTagsToActivate(Tag);
			AbilitySystemComponent->TryActivateAbilitiesByTag(AbilityTagsToActivate);
		}
	}
	
	inline void DrawTrace(UWorld* World, FVector Start, FVector End, FColor Color)
	{
		DrawDebugSphere(World, Start, 20.f, 6, Color, false, 2.f);
		DrawDebugLine(World, Start, End, Color, false, 2.f);
		DrawDebugSphere(World, End, 20.f, 6, Color, false, 2.f);
	}
}
