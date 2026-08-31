#pragma once


#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"

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
}