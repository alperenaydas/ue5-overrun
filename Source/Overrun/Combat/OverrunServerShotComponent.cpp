// Fill out your copyright notice in the Description page of Project Settings.

#include "Overrun/Combat/OverrunServerShotComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemLog.h"
#include "Overrun/AbilitySystem/OverrunGameplayTags.h"
#include "Overrun/Character/TopDownCharacter.h"
#include "Engine/World.h"
#include "Overrun/Mass/OverrunEnemyDamageSubsystem.h"
#include "Overrun/Movement/TopDownCMC.h"

UOverrunServerShotComponent::UOverrunServerShotComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UOverrunServerShotComponent::SubmitShot(const FFireAbilityData& Shot)
{
	ATopDownCharacter* Character = Cast<ATopDownCharacter>(GetOwner());
	UWorld* World = GetWorld();

	if (!IsValid(Character) || !Character->HasAuthority() || !World || Character->IsDead())
	{
		return;
	}
	
	ExpirePendingShots();

	if (GetNumQueuedShots() >= 16)
	{
		UE_LOG(LogAbilitySystem, Verbose, TEXT("Shot rejected: server shot queue full."));
		return;
	}
	
	if (Character->IsLocallyControlled())
	{
		QueueResolvedShot(Shot, Character->GetActorLocation());
		return;
	}
	
	UTopDownCMC* CMC = SubscribedCMC.Get();
	if (!CMC)
	{
		UE_LOG(LogAbilitySystem, Error, TEXT("Server shot rejected: movement component unavailable."));
		return;
	}

	FCharacterMovementDataForHistoryBuffer MovementData;
	if (CMC->TryFindMovementHistory(Shot.CMCMovementTimestamp, MovementData))
	{
		QueueResolvedShot(Shot, MovementData.ServerSimulatedLocation);
		return;
	}

	PendingHistoryShots.Add(Shot);

	if (!World->GetTimerManager().IsTimerActive(PendingShotCleanupTimerHandle))
	{
		World->GetTimerManager().SetTimer(
			PendingShotCleanupTimerHandle,
			this,
			&UOverrunServerShotComponent::ExpirePendingShots,
			PendingShotCleanupInterval,
			true);
	}
}

void UOverrunServerShotComponent::CancelPendingShots()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ResolvedShotTimerHandle);
		World->GetTimerManager().ClearTimer(PendingShotCleanupTimerHandle);
	}

	ResolvedShots.Reset();
	PendingHistoryShots.Reset();
}


void UOverrunServerShotComponent::BeginPlay()
{
	Super::BeginPlay();
	ATopDownCharacter* Character = Cast<ATopDownCharacter>(GetOwner());
	if (!IsValid(Character) || !Character->HasAuthority())
	{
		return;
	}

	UTopDownCMC* CMC =
		Cast<UTopDownCMC>(Character->GetCharacterMovement());

	if (!CMC)
	{
		UE_LOG(LogAbilitySystem, Error,
			TEXT("Server shot component requires UTopDownCMC."));
		return;
	}
	SubscribedCMC = CMC;
	HistoryDelegateHandle = CMC->OnMovementHistoryRecorded.AddUObject(this, &UOverrunServerShotComponent::OnMovementHistoryRecorded);
}

void UOverrunServerShotComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UTopDownCMC* CMC = SubscribedCMC.Get())
	{
		CMC->OnMovementHistoryRecorded.Remove(HistoryDelegateHandle);
	}

	HistoryDelegateHandle.Reset();
	SubscribedCMC.Reset();
	
	CancelPendingShots();
	Super::EndPlay(EndPlayReason);
}

void UOverrunServerShotComponent::ExpirePendingShots()
{
	UWorld* World = GetWorld();
	const AActor* Owner = GetOwner();

	if (!World || !IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	for (int32 i = PendingHistoryShots.Num() - 1; i >= 0; --i)
	{
		if (Now - PendingHistoryShots[i].ServerReceiptTime >= FFireAbilityData::MaxLifetimeSeconds)
		{
			UE_LOG(LogAbilitySystem, Log, TEXT("Pending shot expired. Movement timestamp: %f"), PendingHistoryShots[i].CMCMovementTimestamp);
			PendingHistoryShots.RemoveAt(i);
		}
	}

	if (PendingHistoryShots.IsEmpty())
	{
		World->GetTimerManager().ClearTimer(PendingShotCleanupTimerHandle);
	}
}

void UOverrunServerShotComponent::ProcessResolvedShots()
{
	ResolvedShotTimerHandle.Invalidate();
	const TArray<FResolvedShot> ShotsToProcess = MoveTemp(ResolvedShots);
	for (const FResolvedShot& Shot : ShotsToProcess)
	{
		ValidateAndApplyServerShot(Shot.Request, Shot.ReferenceOrigin);
	}
}

void UOverrunServerShotComponent::OnMovementHistoryRecorded(float Timestamp)
{
	ExpirePendingShots();
	if (PendingHistoryShots.Num() == 0) return;
	if (UTopDownCMC* CMC = SubscribedCMC.Get())
	{
		FCharacterMovementDataForHistoryBuffer CharacterMovementData;
		if (CMC->TryFindMovementHistory(Timestamp, CharacterMovementData))
		{
			for (int32 i = 0; i < PendingHistoryShots.Num();)
			{
				if (PendingHistoryShots[i].CMCMovementTimestamp == Timestamp)
				{
					const FFireAbilityData Request = PendingHistoryShots[i];
					PendingHistoryShots.RemoveAt(i);
					QueueResolvedShot(Request, CharacterMovementData.ServerSimulatedLocation);
					UE_LOG(LogAbilitySystem, Verbose, TEXT("Shot resolved from movement history. Timestamp: %f, Wait: %.3f s"), Timestamp, GetWorld()->GetTimeSeconds() - Request.ServerReceiptTime);
				}
				else
				{
					++i;
				}
			}
		}
	}
	if (PendingHistoryShots.IsEmpty())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(PendingShotCleanupTimerHandle);
		}
	}
}

void UOverrunServerShotComponent::ValidateAndApplyServerShot(const FFireAbilityData& Shot, const FVector& ReferenceOrigin)
{
	ATopDownCharacter* Character = Cast<ATopDownCharacter>(GetOwner());
	UWorld* World = GetWorld();

	if (!IsValid(Character) || !Character->HasAuthority() || !World)
	{
		return;
	}

	UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent();
	if (!ASC || Character->IsDead() || ASC->HasMatchingGameplayTag(TAG_State_Dead))
	{
		return;
	}
	// Waiting for movement does not extend a shot's lifetime.
	const double RequestAge = World->GetTimeSeconds() - Shot.ServerReceiptTime;
	if (!FMath::IsFinite(RequestAge) || RequestAge < 0.0 || RequestAge >= FFireAbilityData::MaxLifetimeSeconds)
	{
		UE_LOG(LogAbilitySystem, Verbose, TEXT("Shot rejected at processing: receipt age %.3f s. Character: %s, Timestamp: %f"), RequestAge, *Character->GetName(), Shot.CMCMovementTimestamp);
		return;
	}
	const FVector SampledStart = Shot.SampledOrigin;
	const FVector SampledDirectionVector = Shot.SampledDestination - SampledStart;
	if (SampledStart.ContainsNaN() || ReferenceOrigin.ContainsNaN()
		|| SampledDirectionVector.ContainsNaN() || SampledDirectionVector.IsNearlyZero()) return;

	if (!FMath::IsFinite(Shot.MaxOriginError) || Shot.MaxOriginError < 0.f)
	{
		return;
	}

	const double OriginErrorSquared = FVector::DistSquared(SampledStart, ReferenceOrigin);

	if (OriginErrorSquared > Shot.MaxOriginError * Shot.MaxOriginError)
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
			Character->MulticastShotCosmetics(SampledStart + SampledUnitDirection * EnemyDistance);
			FOverrunEnemyDamageCommand Command;
			Command.TargetEntity = EnemyEntity;
			Command.Damage = Shot.Damage;

			if (!DamageSubsystem->EnqueueDamage(Command))
			{
				UE_LOG(LogAbilitySystem, Warning, TEXT("Enemy damage command rejected."));
			}

			return;
		}
	}
	Character->MulticastShotCosmetics(Hit.bBlockingHit ? Hit.ImpactPoint : ShotEnd);
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
					GESpecHandle.Data->SetSetByCallerMagnitude(TAG_Data_HealthDelta, -Shot.Damage);
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

void UOverrunServerShotComponent::QueueResolvedShot(const FFireAbilityData& Shot, const FVector& ReferenceOrigin)
{
	ATopDownCharacter* Character = Cast<ATopDownCharacter>(GetOwner());
	UWorld* World = GetWorld();

	if (!IsValid(Character) || !Character->HasAuthority()
		|| !World || Character->IsDead())
	{
		return;
	}

	if (GetNumQueuedShots() >= 16)
	{
		UE_LOG(LogAbilitySystem, Verbose, TEXT("Resolved-shot queue full."));
		return;
	}
	
	ResolvedShots.Add(FResolvedShot{Shot, ReferenceOrigin});

	if (!ResolvedShotTimerHandle.IsValid())
	{
		ResolvedShotTimerHandle =World->GetTimerManager().SetTimerForNextTick(this, &UOverrunServerShotComponent::ProcessResolvedShots);
	}
}
