// This source code is licensed under the MIT license found in the LICENSE file in the root directory of this source tree.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "Math/Bounds.h"

#include "TPCATypes.generated.h"

extern TPCA_API const FName NAME_Spectator;
extern TPCA_API const FName NAME_Normal;
extern TPCA_API const FName NAME_Ragdoll;
extern TPCA_API const FName NAME_Crouched;
extern TPCA_API const FName NAME_Standing;
extern TPCA_API const FName NAME_Primary;
extern TPCA_API const FName NAME_Secondary;

 /** */
UENUM(BlueprintType)
enum class ETurnInPlaceState : uint8
{
	Done,
	InProgress,
	Suspended
};

UENUM(BlueprintType)
enum class ECharacterGait : uint8
{
	Walk,
	Run,
	Sprint
};

UENUM(BlueprintType)
enum class ECharacterRotationMode : uint8
{
	/** Do not perform any automatic actor rotation. */
	None						UMETA(DisplayName = "None"),
	/** Rotate actor towards its direction of movemment. */
	OrientToMovement			UMETA(DisplayName = "Orient to Movement"),
	/** Rotate actor in increments of 90 degrees after the angular distance to the control rotation goes beyond the limit. */
	OrientToController			UMETA(DisplayName = "Orient to Controller"),
};

/** Helper function for net serialization of FVector */
bool TPCA_API SerializeQuantizedVector(FArchive& Ar, FVector& Vector, EVectorQuantization QuantizationLevel);

/** Helper function for net serialization of FRotator */
void TPCA_API SerializeQuantizedRotator(FArchive& Ar, FRotator& Rotator, ERotatorQuantization QuantizationLevel);

/**
 * Replicated look rotation.
 * Struct used for configurable replication precision.
 */
USTRUCT()
struct TPCA_API FRepLook
{
	GENERATED_BODY()

	FRepLook()
		: RotationQuantizationLevel(ERotatorQuantization::ByteComponents)
		, Rotation(ForceInitToZero)
	{}

private:

	/** Allows tuning the compression level for replicated rotation. You should only need to change this from the default if you see visual artifacts. */
	UPROPERTY(EditDefaultsOnly, Category = Replication, AdvancedDisplay)
	ERotatorQuantization RotationQuantizationLevel;

public:

	UPROPERTY(Transient)
	FRotator Rotation;

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	{
		bOutSuccess = true;
		SerializeQuantizedRotator(Ar, Rotation, RotationQuantizationLevel);
		return true;
	}

	bool operator==(const FRepLook& Other) const
	{
		return Rotation == Other.Rotation;
	}

	bool operator!=(const FRepLook& Other) const
	{
		return !(*this == Other);
	}
};

template<>
struct TStructOpsTypeTraits<FRepLook>: public TStructOpsTypeTraitsBase2<FRepLook>
{
	enum
	{
		WithNetSerializer = true,
	};
};

/**
 * Replacement for FRepMovement that replicates acceleration normal, pivot turn state and turn in place target
 */
USTRUCT()
struct TPCA_API FRepExtMovement
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	uint8 bIsPivotTurning : 1;

	UPROPERTY(Transient)
	FVector Location;

	UPROPERTY(Transient)
	FRotator Rotation;

	UPROPERTY(Transient)
	FVector Velocity;

	UPROPERTY(Transient)
	FVector Acceleration;

	UPROPERTY(Transient)
	float TurnInPlaceTargetYaw;

	/** Allows tuning the compression level for the replicated velocity vector. You should only need to change this from the default if you see visual artifacts. */
	UPROPERTY(EditDefaultsOnly, Category = Replication, AdvancedDisplay)
	EVectorQuantization VelocityQuantizationLevel;

	/** Allows tuning the compression level for the replicated location vector. You should only need to change this from the default if you see visual artifacts. */
	UPROPERTY(EditDefaultsOnly, Category = Replication, AdvancedDisplay)
	EVectorQuantization LocationQuantizationLevel;

	/** Allows tuning the compression level for replicated rotation. You should only need to change this from the default if you see visual artifacts. */
	UPROPERTY(EditDefaultsOnly, Category = Replication, AdvancedDisplay)
	ERotatorQuantization RotationQuantizationLevel;

	FRepExtMovement()
		: bIsPivotTurning(false)
		, Location(ForceInitToZero)
		, Rotation(ForceInitToZero)
		, Velocity(ForceInitToZero)
		, Acceleration(ForceInitToZero)
		, TurnInPlaceTargetYaw(0.f)
		, VelocityQuantizationLevel(EVectorQuantization::RoundWholeNumber)
		, LocationQuantizationLevel(EVectorQuantization::RoundWholeNumber)
		, RotationQuantizationLevel(ERotatorQuantization::ByteComponents)
	{}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	{
		// Pack bitfield with flags
		uint8 Flags = (bIsPivotTurning << 0);
		Ar.SerializeBits(&Flags, 1);
		bIsPivotTurning = (Flags & (1 << 0)) ? 1 : 0;

		bOutSuccess = true;

		bOutSuccess &= SerializeQuantizedVector(Ar, Location, LocationQuantizationLevel);
		SerializeQuantizedRotator(Ar, Rotation, RotationQuantizationLevel);
		bOutSuccess &= SerializeQuantizedVector(Ar, Velocity, VelocityQuantizationLevel);
		bOutSuccess &= SerializeFixedVector<1, 16>(Acceleration, Ar);

		Ar << TurnInPlaceTargetYaw;

		return true;
	}

	bool operator==(const FRepExtMovement& Other) const
	{
		return bIsPivotTurning == Other.bIsPivotTurning
			&& Location == Other.Location
			&& Rotation == Other.Rotation
			&& Velocity == Other.Velocity
			&& Acceleration == Other.Acceleration
			&& TurnInPlaceTargetYaw == Other.TurnInPlaceTargetYaw;
	}

	bool operator!=(const FRepExtMovement& Other) const
	{
		return !(*this == Other);
	}
};

template<>
struct TStructOpsTypeTraits<FRepExtMovement>: public TStructOpsTypeTraitsBase2<FRepExtMovement>
{
	enum
	{
		WithNetSerializer = true,
	};
};
