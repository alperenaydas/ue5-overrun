// Fill out your copyright notice in the Description page of Project Settings.


#include "Overrun/AbilitySystem/GA_Fire.h"

#include "GameplayPrediction.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemLog.h"
#include "FGameplayAbilityTargetData_Shot.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "OverrunGameplayTags.h"
#include "Overrun/OverrunHelper.h"
#include "Overrun/Character/TopDownCharacter.h"
#include "Overrun/Combat/OverrunServerShotComponent.h"
#include "Overrun/Combat/OverrunServerShotTypes.h"
#include "Overrun/Movement/TopDownCMC.h"
#include "HAL/IConsoleManager.h"

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

void UGA_Fire::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (Handle.IsValid() && TargetDataDelegateHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey()).Remove(TargetDataDelegateHandle);
		}
	}

	TargetDataDelegateHandle.Reset();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FireTimerHandle);
	}

	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled);
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
	FVector ShotOrigin = TopDownCharacter->GetActorLocation();
	float LatestLocalMoveTimestamp = 0.f;
	FVector LatestLocalMoveLocation = FVector::Zero();
	if (!ActorInfo->IsNetAuthority())
	{
		UTopDownCMC* CMC = Cast<UTopDownCMC>(TopDownCharacter->GetCharacterMovement());
		if (!CMC) return;
		if (!CMC->TryCaptureShotMovementSnapshot(LatestLocalMoveTimestamp, LatestLocalMoveLocation)) return;
		ShotOrigin = LatestLocalMoveLocation;
	}
	const FPredictionKey ActivationKey = GetCurrentActivationInfo().GetActivationPredictionKey();
	FGameplayAbilityTargetDataHandle TargetData;
	FGameplayAbilityTargetData_Shot* ShotDataInfo = new FGameplayAbilityTargetData_Shot();
	ShotDataInfo->SourceLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
	ShotDataInfo->TargetLocation.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
	ShotDataInfo->MovementTimestamp = LatestLocalMoveTimestamp;
	FVector Forward = TopDownCharacter->GetControlRotation().Vector();
	FVector EndPoint = ShotOrigin + Forward * 2000;
	ShotDataInfo->SourceLocation.LiteralTransform = FTransform(ShotOrigin);
	ShotDataInfo->TargetLocation.LiteralTransform = FTransform(EndPoint);
	TargetData.Add(ShotDataInfo);
	FScopedPredictionWindow PredictionWindow(ASC, !ActorInfo->IsNetAuthority());
	static IConsoleVariable* ShotTracesCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("Overrun.Debug.ShotTraces"));
	if (ShotTracesCVar && ShotTracesCVar->GetInt() == 1)
	{
		FHitResult Hit;
		FCollisionQueryParams CollisionParams = FCollisionQueryParams();
		CollisionParams.AddIgnoredActor(TopDownCharacter);
		CollisionParams.bTraceComplex = false;
		GetWorld()->LineTraceSingleByChannel(Hit, ShotOrigin, EndPoint, ECC_Visibility, CollisionParams);
		if (Hit.bBlockingHit)
		{
			OverrunHelper::DrawTrace(GetWorld(), ShotOrigin, Hit.ImpactPoint, FColor::Green);
		}
		else
		{
			OverrunHelper::DrawTrace(GetWorld(), ShotOrigin, EndPoint, FColor::Green);
		}
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
	if (!IsActive() || !ActorInfo || !ActorInfo->IsNetAuthority())
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
	if (ReceivedData.Num() != 1 || !Entry || Entry->GetScriptStruct() != FGameplayAbilityTargetData_Shot::StaticStruct())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const FGameplayAbilityTargetData_Shot* ShotData = static_cast<const FGameplayAbilityTargetData_Shot*>(Entry);
	if (ShotData->SourceLocation.LocationType != EGameplayAbilityTargetingLocationType::LiteralTransform
		|| ShotData->TargetLocation.LocationType != EGameplayAbilityTargetingLocationType::LiteralTransform
		|| !FMath::IsFinite(ShotData->MovementTimestamp) || ShotData->MovementTimestamp < 0.f)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FVector SampledStart = ShotData->GetOrigin().GetLocation();
	const FVector SampledEnd = ShotData->GetEndPoint();
	const float MovementTimestamp = ShotData->MovementTimestamp;
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
	UWorld* World = GetWorld();
	if (!World || Character->IsDead()) return;

	// Charge once on receipt, including requests that subsequently wait or expire.
	if (!TryConsumeServerShotBudget())
	{
		UE_LOG(LogAbilitySystem, Verbose, TEXT("Shot rejected by fire-rate budget. Character: %s, Timestamp: %f, Credits: %.3f"), *Character->GetName(), MovementTimestamp, ServerShotCredits);
		return;
	}
	
	const FFireAbilityData Request{
		SampledStart,
		MovementTimestamp,
		SampledEnd,
		World->GetTimeSeconds(),
		ShotDamage,
		MaxShotOriginError
	};
	
	if (!IsValid(Character->ServerShotComponent))
	{
		UE_LOG(LogAbilitySystem, Error,
			TEXT("Fire ability requires a server shot component."));
		return;
	}

	Character->ServerShotComponent->SubmitShot(Request);
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