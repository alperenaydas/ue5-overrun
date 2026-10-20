#pragma once

#include "CoreMinimal.h"
#include "Serialization/Archive.h"
#include "OverrunMovementPacket.generated.h"

class UPackageMap;

namespace OverrunMovement
{
	inline constexpr int32 MaxRecords = 48;
	inline constexpr double Interval = 1.0 / 20.0;
	// These sizes describe the custom payload, excluding RPC and transport overhead.
	inline constexpr int32 SequenceBits = 32;
	inline constexpr int32 CountBits = 6;
	inline constexpr int32 TokenBits = 32;
	inline constexpr int32 SampleTimeBits = 32;
	inline constexpr int32 PoseBits = 64;
	inline constexpr int32 HeaderBits = SequenceBits + CountBits;
	inline constexpr int32 RecordBits = TokenBits + SampleTimeBits + PoseBits;
	inline constexpr int32 HeaderBytes = (HeaderBits + 7) / 8;
	inline constexpr int32 RecordBytes = RecordBits / 8;
	inline constexpr int32 MaxPayloadBytes = (HeaderBits + RecordBits * MaxRecords + 7) / 8;
	static_assert(MaxRecords < (1 << CountBits));
	static_assert(MaxPayloadBytes == 773);

	// Unsigned representation of signed 18-bit centimetre coordinates.
	inline constexpr int64 CoordinateBias = 131072;
	inline constexpr uint64 CoordinateMask = (uint64(1) << 18) - 1;

	inline bool TryPackPose(const FTransform& Transform, uint64& OutPose)
	{
		const FVector Position = Transform.GetLocation();
		const double Yaw = Transform.Rotator().Yaw;

		if (Position.ContainsNaN() || !FMath::IsFinite(Yaw))
		{
			return false;
		}

		const auto InRange = [](double Value)
		{
			return Value >= -131072.0 && Value <= 131071.0;
		};

		if (!InRange(Position.X) ||
			!InRange(Position.Y) ||
			!InRange(Position.Z))
		{
			return false;
		}

		const uint64 X = uint64(FMath::RoundToInt64(Position.X) + CoordinateBias);
		const uint64 Y = uint64(FMath::RoundToInt64(Position.Y) + CoordinateBias);
		const uint64 Z = uint64(FMath::RoundToInt64(Position.Z) + CoordinateBias);

		const double NormalYaw = FRotator::ClampAxis(Yaw);
		const uint64 QuantizedYaw =
			uint64(FMath::RoundToInt64(NormalYaw * 1024.0 / 360.0)) & 1023;

		OutPose = X | (Y << 18) | (Z << 36) | (QuantizedYaw << 54);
		return true;
	}

	inline void UnpackPose(
		uint64 Pose, FVector& OutPosition, FQuat& OutRotation)
	{
		const auto DecodeCoordinate = [](uint64 Value)
		{
			return double(int64(Value & CoordinateMask) - CoordinateBias);
		};

		OutPosition = FVector(
			DecodeCoordinate(Pose),
			DecodeCoordinate(Pose >> 18),
			DecodeCoordinate(Pose >> 36));

		const double Yaw = double((Pose >> 54) & 1023) * 360.0 / 1024.0;
		OutRotation = FRotator(0.0, Yaw, 0.0).Quaternion();
	}

	inline bool IsNewerSequence(uint32 Candidate, uint32 Previous)
	{
		const uint32 Difference = Candidate - Previous;
		return Difference != 0 && Difference < 0x80000000u;
	}
}

struct FOverrunMovementRecord
{
	uint32 Token = 0;
	uint32 SampleTimeMs = 0;
	uint64 Pose = 0;
};

USTRUCT()
struct FOverrunMovementPacket
{
	GENERATED_BODY()

	uint32 Sequence = 0;
	TArray<FOverrunMovementRecord> Records;

	bool NetSerialize(FArchive& Ar, UPackageMap*, bool& bOutSuccess)
	{
		if (Ar.IsSaving() && Records.Num() > OverrunMovement::MaxRecords)
		{
			Ar.SetError();
			bOutSuccess = false;
			return true;
		}

		Ar.SerializeBits(&Sequence, OverrunMovement::SequenceBits);

		uint8 Count = Ar.IsSaving() ? uint8(Records.Num()) : 0;
		Ar.SerializeBits(&Count, OverrunMovement::CountBits);

		if (Count > OverrunMovement::MaxRecords)
		{
			Ar.SetError();
			bOutSuccess = false;
			return true;
		}

		if (Ar.IsLoading())
		{
			Records.SetNum(Count);
		}

		for (FOverrunMovementRecord& Record : Records)
		{
			Ar.SerializeBits(&Record.Token, OverrunMovement::TokenBits);
			Ar.SerializeBits(&Record.SampleTimeMs, OverrunMovement::SampleTimeBits);
			Ar.SerializeBits(&Record.Pose, OverrunMovement::PoseBits);
		}

		bOutSuccess = !Ar.IsError();
		return true;
	}
};

template<>
struct TStructOpsTypeTraits<FOverrunMovementPacket>
	: TStructOpsTypeTraitsBase2<FOverrunMovementPacket>
{
	enum
	{
		WithNetSerializer = true
	};
};
