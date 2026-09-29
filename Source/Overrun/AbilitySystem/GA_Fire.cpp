// Fill out your copyright notice in the Description page of Project Settings.


#include "Overrun/AbilitySystem/GA_Fire.h"

#include "GameplayPrediction.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemLog.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "OverrunGameplayTags.h"
#include "Overrun/OverrunHelper.h"
#include "Overrun/Character/TopDownCharacter.h"

UGA_Fire::UGA_Fire()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	const FGameplayTagContainer AbilityTagsToSet(TAG_Ability_Fire);
	SetAssetTags(AbilityTagsToSet);
	bServerRespectsRemoteAbilityCancellation = true;
}

void UGA_Fire::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	TWeakObjectPtr<AActor> Avatar = ActorInfo->AvatarActor;
	if (ATopDownCharacter* TopDownCharacter = Cast<ATopDownCharacter>(Avatar.Get()))
	{
		if (TopDownCharacter->IsDead())
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
		UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
		const FPredictionKey ActivationKey = ActivationInfo.GetActivationPredictionKey();
		if (ActorInfo->IsNetAuthority() && !ActorInfo->IsLocallyControlled()) // non-local server actor instance
		{
			TargetDataDelegateHandle = ASC->AbilityTargetDataSetDelegate(Handle, ActivationKey).AddUObject(this, &UGA_Fire::OnTargetDataReceived);
			ASC->CallReplicatedTargetDataDelegatesIfSet(Handle, ActivationKey);
			return;
		}
		if (ActorInfo->IsLocallyControlled())
		{
			GetWorld()->GetTimerManager().SetTimer(
				FireTimerHandle,
				this,
				&UGA_Fire::FireLocalShot,
				FireInterval,
				true);
			FireLocalShot();
		}
		return;
	}
	UE_LOG(LogAbilitySystem, Warning, TEXT("Fire ability requires a TopDownCharacter avatar. Ability cancelled."));
	constexpr bool bReplicateEndAbility = true;
	constexpr bool bWasCancelled = true;
	EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Fire::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (Handle.IsValid() && TargetDataDelegateHandle.IsValid())
	{
		UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
		if (ASC)
		{
			ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey()).Remove(TargetDataDelegateHandle);
			TargetDataDelegateHandle.Reset();
		}
	}
	UWorld* World = GetWorld();
	if (World && FireTimerHandle.IsValid())
	{
		World->GetTimerManager().ClearTimer(FireTimerHandle);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	
}

void UGA_Fire::FireLocalShot()
{
	if (!IsActive()) return;
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ActorInfo || !ASC) return;
	if (!ActorInfo->IsLocallyControlled()) return;
	const FGameplayAbilitySpecHandle Handle = GetCurrentAbilitySpecHandle();
	const ATopDownCharacter* TopDownCharacter = Cast<ATopDownCharacter>(ActorInfo->AvatarActor.Get());
	if (!TopDownCharacter || TopDownCharacter->IsDead())
	{
		EndAbility(Handle, ActorInfo, GetCurrentActivationInfo(), true, false);
		return;
	}
	const FPredictionKey ActivationKey = GetCurrentActivationInfo().GetActivationPredictionKey();
	FGameplayAbilityTargetDataHandle TargetData;
	FGameplayAbilityTargetData_LocationInfo* LocationInfo = new FGameplayAbilityTargetData_LocationInfo();
	LocationInfo->SourceLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
	LocationInfo->TargetLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
	FVector CharacterPosition = TopDownCharacter->GetActorLocation();
	FVector Forward = TopDownCharacter->GetControlRotation().Vector();
	FVector EndPoint = CharacterPosition + Forward * 2000;
	LocationInfo->SourceLocation.LiteralTransform = FTransform(CharacterPosition);
	LocationInfo->TargetLocation.LiteralTransform = FTransform(EndPoint);
	TargetData.Add(LocationInfo);
	FScopedPredictionWindow PredictionWindow(ASC, !ActorInfo->IsNetAuthority());
	FHitResult Hit;
	FCollisionQueryParams CollisionParams = FCollisionQueryParams();
	CollisionParams.AddIgnoredActor(TopDownCharacter);
	CollisionParams.bTraceComplex = false;
	GetWorld()->LineTraceSingleByChannel(Hit, CharacterPosition, EndPoint, ECC_Visibility, CollisionParams);
	if (Hit.bBlockingHit)
	{
		OverrunHelper::DrawTrace(GetWorld(), CharacterPosition, Hit.ImpactPoint, FColor::Green); // green represents client
	}
	else
	{
		OverrunHelper::DrawTrace(GetWorld(), CharacterPosition, EndPoint, FColor::Green); // green represents client
	}
	if (ActorInfo->IsNetAuthority())
	{
		OnTargetDataReceived(TargetData, FGameplayTag());
	}
	else
	{
		ASC->CallServerSetReplicatedTargetData(Handle, ActivationKey, TargetData, FGameplayTag(), ASC->ScopedPredictionKey);
	}
}

void UGA_Fire::OnTargetDataReceived(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (!ActorInfo || !ActorInfo->IsNetAuthority())
	{
		return;
	}
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	const FGameplayAbilitySpecHandle Handle = GetCurrentAbilitySpecHandle();
	const FGameplayAbilityActivationInfo ActivationInfo = GetCurrentActivationInfo();
	
	if (!ASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	FGameplayAbilityTargetDataHandle ReceivedData = Data;
	ASC->ConsumeClientReplicatedTargetData(Handle, ActivationInfo.GetActivationPredictionKey());
	
	if (ASC->HasMatchingGameplayTag(TAG_State_Dead))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	const FGameplayAbilityTargetData* Entry = ReceivedData.Get(0);
	if (ReceivedData.Num() != 1 || !Entry || Entry->GetScriptStruct() != FGameplayAbilityTargetData_LocationInfo::StaticStruct())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	const FVector SampledStart = Entry->GetOrigin().GetLocation();
	const FVector SampledEnd = Entry->GetEndPoint();
	if (SampledStart.ContainsNaN() || SampledEnd.ContainsNaN())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const FVector SampledDirectionVector = SampledEnd - SampledStart;
	if (SampledDirectionVector.ContainsNaN() || SampledDirectionVector.IsNearlyZero())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	ATopDownCharacter* Character = Cast<ATopDownCharacter>(ActorInfo->AvatarActor.Get());
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	const FVector ServerPosition = Character->GetActorLocation();

	if (!FMath::IsFinite(MaxShotOriginError) || MaxShotOriginError < 0.f)
	{
		return;
	}

	const double OriginErrorSquared =
		FVector::DistSquared(SampledStart, ServerPosition);

	if (OriginErrorSquared > MaxShotOriginError * MaxShotOriginError)
	{
		UE_LOG(LogAbilitySystem, Warning, TEXT("Character MaxErrorDistance exceeded, shot ignored. ErrorDistanceSqr: %f"), OriginErrorSquared);
		return;
	}
	
	if (!TryConsumeServerShotBudget()) return;
	FVector SampledUnitDirection = SampledDirectionVector.GetSafeNormal();
	FHitResult Hit;
	FCollisionQueryParams CollisionParams = FCollisionQueryParams();
	CollisionParams.AddIgnoredActor(Character);
	CollisionParams.bTraceComplex = false;
	GetWorld()->LineTraceSingleByChannel(Hit, SampledStart, SampledStart + SampledUnitDirection * 2000.f, ECC_Visibility, CollisionParams);
	if (Hit.bBlockingHit)
	{
		if (IAbilitySystemInterface* Target = Cast<IAbilitySystemInterface>(Hit.GetActor()))
		{
			if (UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent())
			{
				FGameplayEffectContextHandle GEContextHandle = ASC->MakeEffectContext();
				FGameplayEffectSpecHandle GESpecHandle = ASC->MakeOutgoingSpec(Character->DamageEffect, 1, GEContextHandle);
				if (GESpecHandle.IsValid())
				{
					ASC->ApplyGameplayEffectSpecToTarget(*GESpecHandle.Data.Get(), TargetASC);
				}
			}
		}
	}
}

bool UGA_Fire::TryConsumeServerShotBudget()
{
	const UWorld* World = GetWorld();
	if (!World || !FMath::IsFinite(FireInterval) || FireInterval <= 0.f)
	{
		return false;
	}

	const double Now = World->GetTimeSeconds();

	if (!bServerShotBudgetInitialized)
	{
		ServerLastBudgetTime = Now;
		bServerShotBudgetInitialized = true;
	}

	const double Elapsed =
		FMath::Max(0.0, Now - ServerLastBudgetTime);

	ServerShotCredits = FMath::Min(
		2.0,
		ServerShotCredits + Elapsed / static_cast<double>(FireInterval));

	ServerLastBudgetTime = Now;

	if (ServerShotCredits < 1.0)
	{
		return false;
	}

	ServerShotCredits -= 1.0;
	return true;
}