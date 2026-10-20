#pragma once

#include "CoreMinimal.h"
#include "MassEntityHandle.h"

class FOverrunCrowdSeparation
{
public:
	static constexpr int32 MaxNeighbors = 8;
	static constexpr double CellSize = 128.0;
	static constexpr double MaxPredictionDistance = 64.0;

	struct FSample
	{
		FMassEntityHandle Entity;
		FVector2D Position = FVector2D::ZeroVector;
		FVector2D Velocity = FVector2D::ZeroVector;
		double Radius = 40.0;
	};

	struct FNeighbor
	{
		int32 Index = INDEX_NONE;
		double DistanceSquared = 0.0;
	};

	void Reset()
	{
		Samples.Reset();
		SampleIndices.Reset();
		Cells.Reset();
		MaxRadius = 0.0;
	}

	void Add(const FSample& Sample)
	{
		const int32 Index = Samples.Add(Sample);
		SampleIndices.Add(Sample.Entity, Index);
		Cells.FindOrAdd(ToCell(Sample.Position)).Add(Index);
		MaxRadius = FMath::Max(MaxRadius, Sample.Radius);
	}

	const int32* FindIndex(FMassEntityHandle Entity) const { return SampleIndices.Find(Entity); }
	const FSample& GetSample(int32 Index) const { return Samples[Index]; }

	int32 FindNeighbors(int32 Index, double Padding, FNeighbor (&Out)[MaxNeighbors]) const
	{
		const FSample& Sample = Samples[Index];
		const double SearchRadius = Sample.Radius + MaxRadius + Padding + MaxPredictionDistance;
		const FIntPoint Min = ToCell(Sample.Position - FVector2D(SearchRadius, SearchRadius));
		const FIntPoint Max = ToCell(Sample.Position + FVector2D(SearchRadius, SearchRadius));
		int32 Count = 0;
		for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
		{
			for (int32 X = Min.X; X <= Max.X; ++X)
			{
				const FCell* Cell = Cells.Find(FIntPoint(X, Y));
				if (!Cell)
				{
					continue;
				}
				for (const int32 OtherIndex : *Cell)
				{
					if (OtherIndex == Index)
					{
						continue;
					}
					const FSample& Other = Samples[OtherIndex];
					const double DistanceSquared = (Sample.Position - Other.Position).SizeSquared();
					if (DistanceSquared >= FMath::Square(Sample.Radius + Other.Radius + Padding + MaxPredictionDistance))
					{
						continue;
					}
					int32 InsertAt = Count;
					while (InsertAt > 0 && (DistanceSquared < Out[InsertAt - 1].DistanceSquared ||
						(DistanceSquared == Out[InsertAt - 1].DistanceSquared && Other.Entity < Samples[Out[InsertAt - 1].Index].Entity)))
					{
						--InsertAt;
					}
					if (InsertAt >= MaxNeighbors)
					{
						continue;
					}
					for (int32 Slot = FMath::Min(Count, MaxNeighbors - 1); Slot > InsertAt; --Slot)
					{
						Out[Slot] = Out[Slot - 1];
					}
					Out[InsertAt] = { OtherIndex, DistanceSquared };
					Count = FMath::Min(Count + 1, MaxNeighbors);
				}
			}
		}
		return Count;
	}

	FVector CalculateAcceleration(int32 Index, double Padding, double Stiffness, double MaxAcceleration) const
	{
		FNeighbor Neighbors[MaxNeighbors];
		const int32 Count = FindNeighbors(Index, Padding, Neighbors);
		const FSample& Sample = Samples[Index];
		FVector2D Acceleration = FVector2D::ZeroVector;
		for (int32 Slot = 0; Slot < Count; ++Slot)
		{
			const FNeighbor& Neighbor = Neighbors[Slot];
			const FSample& Other = Samples[Neighbor.Index];
			const double Distance = FMath::Sqrt(Neighbor.DistanceSquared);
			FVector2D Direction;
			if (Distance > 0.001)
			{
				Direction = (Sample.Position - Other.Position) / Distance;
			}
			else
			{
				const bool bFirst = Sample.Entity < Other.Entity;
				const uint32 Hash = bFirst ? HashCombineFast(GetTypeHash(Sample.Entity), GetTypeHash(Other.Entity))
					: HashCombineFast(GetTypeHash(Other.Entity), GetTypeHash(Sample.Entity));
				const double Angle = double(Hash & 65535u) * (2.0 * PI / 65536.0);
				Direction = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * (bFirst ? 1.0 : -1.0);
			}
			const double ClosingSpeed = FMath::Max(0.0, -FVector2D::DotProduct(Sample.Velocity - Other.Velocity, Direction));
			const double Prediction = FMath::Min(MaxPredictionDistance, ClosingSpeed * 0.25);
			const double Penetration = Sample.Radius + Other.Radius + Padding + Prediction - Distance;
			if (Penetration > 0.0)
			{
				Acceleration += Direction * (Penetration * Stiffness + ClosingSpeed * 8.0);
			}
		}
		return FVector(Acceleration.X, Acceleration.Y, 0.0).GetClampedToMaxSize(MaxAcceleration);
	}

private:
	using FCell = TArray<int32, TInlineAllocator<8>>;
	static FIntPoint ToCell(const FVector2D& Position)
	{
		return FIntPoint(FMath::FloorToInt(Position.X / CellSize), FMath::FloorToInt(Position.Y / CellSize));
	}
	TArray<FSample> Samples;
	TMap<FMassEntityHandle, int32> SampleIndices;
	TMap<FIntPoint, FCell> Cells;
	double MaxRadius = 0.0;
};
