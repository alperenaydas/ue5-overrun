#pragma once
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "FGameplayAbilityTargetData_Shot.generated.h"

USTRUCT()
struct FGameplayAbilityTargetData_Shot : public FGameplayAbilityTargetData_LocationInfo
{
	GENERATED_BODY()

public:
	float MovementTimestamp = 0.f;
	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); } // for GAS to know our new type

	// handles both serialization and deserialization
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
	{
		const bool bFullyMapped = FGameplayAbilityTargetData_LocationInfo::NetSerialize(Ar, Map, bOutSuccess);

		Ar << MovementTimestamp;

		bOutSuccess = bOutSuccess && !Ar.IsError();
		return bFullyMapped;
	}
};

template<>
struct TStructOpsTypeTraits<FGameplayAbilityTargetData_Shot> : public TStructOpsTypeTraitsBase2<FGameplayAbilityTargetData_Shot>
{
	enum
	{
		WithNetSerializer = true
	};
};
