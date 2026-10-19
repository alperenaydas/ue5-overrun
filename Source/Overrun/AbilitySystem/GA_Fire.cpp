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
#include "Overrun/Movement/TopDownCMC.h"
#include "Overrun/Mass/OverrunEnemyDamageSubsystem.h"
#include "Overrun/Mass/OverrunEnemyDamageCommand.h"

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
			UTopDownCMC* CMC = Cast<UTopDownCMC>(TopDownCharacter->GetCharacterMovement());
			if (!CMC)
			{
				EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
				return;
			}
			HistoryDelegateHandle = CMC->OnMovementHistoryRecorded.AddUObject(this, &UGA_Fire::OnMovementHistoryRecorded);
			SubscribedCMC = CMC;
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
	if (World)
	{
		World->GetTimerManager().ClearTimer(PendingShotCleanupTimerHandle);
		World->GetTimerManager().ClearTimer(ResolvedShotTimerHandle);
	}

	if (UTopDownCMC* CMC = SubscribedCMC.Get())
	{
		CMC->OnMovementHistoryRecorded.Remove(HistoryDelegateHandle);
	}
	HistoryDelegateHandle.Reset();
	SubscribedCMC.Reset();
	if (!FireAbilityDatas.IsEmpty() || !ResolvedShots.IsEmpty())
	{
		UE_LOG(LogAbilitySystem, Verbose, TEXT("Fire ended: discarding %d pending and %d resolved shots."), FireAbilityDatas.Num(), ResolvedShots.Num());
	}
	FireAbilityDatas.Reset();
	ResolvedShots.Reset();
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
	const FFireAbilityData Request{SampledStart, MovementTimestamp, SampledEnd, World->GetTimeSeconds()};
	if (!ActorInfo->IsLocallyControlled())
	{
		ExpirePendingShots();
		UTopDownCMC* CMC = Cast<UTopDownCMC>(Character->GetMovementComponent());
		if (!CMC)
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
		FCharacterMovementDataForHistoryBuffer CharacterMovementDataAtTime;
		if (CMC->TryFindMovementHistory(MovementTimestamp, CharacterMovementDataAtTime))
		{
			QueueResolvedShot(Request, CharacterMovementDataAtTime.ServerSimulatedLocation);
		}
		else
		{
			if (FireAbilityDatas.Num() + ResolvedShots.Num() >= 16)
			{
				UE_LOG(LogAbilitySystem, Verbose, TEXT("Shot rejected: history queue full. Character: %s, Timestamp: %f"), *Character->GetName(), MovementTimestamp);
				return;
			}
			FireAbilityDatas.Add(Request);
			if (!World->GetTimerManager().IsTimerActive(PendingShotCleanupTimerHandle))
			{
				World->GetTimerManager().SetTimer(
					PendingShotCleanupTimerHandle,
					this,
					&UGA_Fire::ExpirePendingShots,
					PendingShotCleanupInterval,
					true);
			}
			UE_LOG(LogAbilitySystem, Verbose, TEXT("Shot waiting for movement history. Character: %s, Timestamp: %f"), *Character->GetName(), MovementTimestamp);
			return;
		}
		return;
	}
	QueueResolvedShot(Request, Character->GetActorLocation());
}

void UGA_Fire::ValidateAndApplyServerShot(const FFireAbilityData& Shot, const FVector& ReferenceOrigin)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	UWorld* World = GetWorld();
	if (!IsActive() || !ActorInfo || !ActorInfo->IsNetAuthority() || !ASC || !World) return;
	ATopDownCharacter* Character = Cast<ATopDownCharacter>(ActorInfo->AvatarActor.Get());
	if (!Character) return;
	if (Character->IsDead() || ASC->HasMatchingGameplayTag(TAG_State_Dead))
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
		return;
	}
	// Waiting for movement does not extend a shot's lifetime.
	const double RequestAge = World->GetTimeSeconds() - Shot.ServerReceiptTime;
	if (!FMath::IsFinite(RequestAge) || RequestAge < 0.0 || RequestAge >= MaxPendingShotWait)
	{
		UE_LOG(LogAbilitySystem, Verbose, TEXT("Shot rejected at processing: receipt age %.3f s. Character: %s, Timestamp: %f"), RequestAge, *Character->GetName(), Shot.CMCMovementTimestamp);
		return;
	}
	const FVector SampledStart = Shot.SampledOrigin;
	const FVector SampledDirectionVector = Shot.SampledDestination - SampledStart;
	if (SampledStart.ContainsNaN() || ReferenceOrigin.ContainsNaN()
		|| SampledDirectionVector.ContainsNaN() || SampledDirectionVector.IsNearlyZero()) return;

	if (!FMath::IsFinite(MaxShotOriginError) || MaxShotOriginError < 0.f)
	{
		return;
	}

	const double OriginErrorSquared = FVector::DistSquared(SampledStart, ReferenceOrigin);

	if (OriginErrorSquared > MaxShotOriginError * MaxShotOriginError)
	{
		UE_LOG(LogAbilitySystem, Warning, TEXT("Character MaxErrorDistance exceeded, shot ignored. ErrorDistanceSqr: %f"), OriginErrorSquared);
		return;
	}
	
	FVector SampledUnitDirection = SampledDirectionVector.GetSafeNormal();
	constexpr float ShotRange = 2000.f;
	const FVector ShotEnd = SampledStart + SampledUnitDirection * ShotRange;
	FHitResult Hit;
	FCollisionQueryParams CollisionParams = FCollisionQueryParams();
	CollisionParams.AddIgnoredActor(Character);
	CollisionParams.bTraceComplex = false;
	World->LineTraceSingleByChannel(Hit, SampledStart, ShotEnd, ECC_Visibility, CollisionParams);
	UE_LOG(LogAbilitySystem, Verbose, TEXT("Server shot trace. Character: %s, Timestamp: %f, Receipt age: %.3f s, Origin error squared: %.3f, Blocking hit: %d, Target: %s"), *Character->GetName(), Shot.CMCMovementTimestamp, RequestAge, OriginErrorSquared, Hit.bBlockingHit ? 1 : 0, *GetNameSafe(Hit.GetActor()));
	const float MaxEnemyHitDistance = Hit.bBlockingHit ? Hit.Distance : ShotRange;
	UE_LOG(LogAbilitySystem, Verbose, TEXT("Enemy query distance limit: %.1f cm"), MaxEnemyHitDistance);
	if (UOverrunEnemyDamageSubsystem* DamageSubsystem = World->GetSubsystem<UOverrunEnemyDamageSubsystem>())
	{
		FMassEntityHandle EnemyEntity;
		float EnemyDistance = 0.f;
		const bool bEnemyHit = DamageSubsystem->FindClosestEnemyHit(SampledStart, SampledUnitDirection, MaxEnemyHitDistance, EnemyEntity, EnemyDistance);
		UE_LOG(LogAbilitySystem, Verbose, TEXT("Mass hit=%d entity=%d:%d distance=%.1f cm"), bEnemyHit, EnemyEntity.Index, EnemyEntity.SerialNumber, bEnemyHit ? EnemyDistance : -1.f);
		if (bEnemyHit && (!Hit.bBlockingHit || EnemyDistance < Hit.Distance))
		{
			FOverrunEnemyDamageCommand Command;
			Command.TargetEntity = EnemyEntity;
			Command.Damage = ShotDamage;

			if (!DamageSubsystem->EnqueueDamage(Command))
			{
				UE_LOG(LogAbilitySystem, Warning, TEXT("Enemy damage command rejected."));
			}

			return;
		}
	}
	if (Hit.bBlockingHit)
	{
		if (IAbilitySystemInterface* Target = Cast<IAbilitySystemInterface>(Hit.GetActor()))
		{
			if (UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent())
			{
				FGameplayEffectContextHandle GEContextHandle = ASC->MakeEffectContext();
				GEContextHandle.AddHitResult(Hit);
				FGameplayEffectSpecHandle GESpecHandle = ASC->MakeOutgoingSpec(Character->DamageEffect, 1, GEContextHandle);
				if (GESpecHandle.IsValid())
				{
					GESpecHandle.Data->SetSetByCallerMagnitude(TAG_Data_HealthDelta, -ShotDamage);
					ASC->ApplyGameplayEffectSpecToTarget(*GESpecHandle.Data.Get(), TargetASC);
					UE_LOG(LogAbilitySystem, Verbose, TEXT("Server shot submitted damage effect. Character: %s, Timestamp: %f, Target: %s"), *Character->GetName(), Shot.CMCMovementTimestamp, *GetNameSafe(Hit.GetActor()));
				}
				else
				{
					UE_LOG(LogAbilitySystem, Warning, TEXT("Server shot could not create damage effect spec. Character: %s"), *Character->GetName());
				}
			}
		}
	}
}

void UGA_Fire::QueueResolvedShot(const FFireAbilityData& Shot, const FVector& ReferenceOrigin)
{
	if (!IsActive()) return;
	if (FireAbilityDatas.Num() + ResolvedShots.Num() >= 16)
	{
		UE_LOG(LogAbilitySystem, Verbose, TEXT("Shot rejected: server shot queue full."));
		return;
	}
	ResolvedShots.Add(FResolvedShot(Shot, ReferenceOrigin));
	if (!ResolvedShotTimerHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			ResolvedShotTimerHandle = World->GetTimerManager().SetTimerForNextTick(this, &UGA_Fire::ProcessResolvedShots);
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

void UGA_Fire::ExpirePendingShots()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	UWorld* World = GetWorld();
	if (!ActorInfo || !ActorInfo->IsNetAuthority() || !World) return;

	const double Now = World->GetTimeSeconds();
	for (int32 i = FireAbilityDatas.Num() - 1; i >= 0; --i)
	{
		if (Now - FireAbilityDatas[i].ServerReceiptTime >= MaxPendingShotWait)
		{
			UE_LOG(LogAbilitySystem, Log, TEXT("Pending shot expired. Movement timestamp: %f"), FireAbilityDatas[i].CMCMovementTimestamp);
			FireAbilityDatas.RemoveAt(i);
		}
	}

	if (FireAbilityDatas.IsEmpty())
	{
		World->GetTimerManager().ClearTimer(PendingShotCleanupTimerHandle);
	}
}

void UGA_Fire::OnMovementHistoryRecorded(float Timestamp)
{
	if (!IsActive()) return;
	ExpirePendingShots();
	if (FireAbilityDatas.Num() == 0) return;
	if (UTopDownCMC* CMC = SubscribedCMC.Get())
	{
		FCharacterMovementDataForHistoryBuffer CharacterMovementData;
		if (CMC->TryFindMovementHistory(Timestamp, CharacterMovementData))
		{
			for (int32 i = 0; i < FireAbilityDatas.Num();)
			{
				if (FireAbilityDatas[i].CMCMovementTimestamp == Timestamp)
				{
					const FResolvedShot ResolvedShot{FireAbilityDatas[i], CharacterMovementData.ServerSimulatedLocation};
					FireAbilityDatas.RemoveAt(i);
					QueueResolvedShot(ResolvedShot.Request, ResolvedShot.ReferenceOrigin);
					UE_LOG(LogAbilitySystem, Verbose, TEXT("Shot resolved from movement history. Timestamp: %f, Wait: %.3f s"), Timestamp, GetWorld()->GetTimeSeconds() - ResolvedShot.Request.ServerReceiptTime);
				}
				else
				{
					++i;
				}
			}
		}
	}
	if (FireAbilityDatas.IsEmpty())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(PendingShotCleanupTimerHandle);
		}
	}
}

void UGA_Fire::ProcessResolvedShots()
{
	ResolvedShotTimerHandle.Invalidate();
	// Detach the batch before effects can cancel the ability and clear its queues.
	const TArray<FResolvedShot> ShotsToProcess = MoveTemp(ResolvedShots);
	const FPredictionKey ActivationKey = GetCurrentActivationInfo().GetActivationPredictionKey();
	for (const FResolvedShot& Shot : ShotsToProcess)
	{
		if (!IsActive() || GetCurrentActivationInfo().GetActivationPredictionKey() != ActivationKey) return;
		ValidateAndApplyServerShot(Shot.Request, Shot.ReferenceOrigin);
	}
}
