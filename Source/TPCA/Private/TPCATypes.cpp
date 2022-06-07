// This source code is licensed under the MIT license found in the LICENSE file in the root directory of this source tree.

#include "TPCATypes.h"

const FName NAME_Spectator(TEXT("Spectator"));
const FName NAME_Normal(TEXT("Normal"));
const FName NAME_Ragdoll(TEXT("Ragdoll"));
const FName NAME_Crouched(TEXT("Crouched"));
const FName NAME_Standing(TEXT("Standing"));
const FName NAME_Primary(TEXT("Primary"));
const FName NAME_Secondary(TEXT("Secondary"));

bool SerializeQuantizedVector(FArchive& Ar, FVector& Vector, EVectorQuantization QuantizationLevel)
{
	// Since FRepMovement used to use FVector_NetQuantize100, we're allowing enough bits per component
	// regardless of the quantization level so that we can still support at least the same maximum magnitude
	// (2^30 / 100, or ~10 million).
	// This uses no inherent extra bandwidth since we're still using the same number of bits to store the
	// bits-per-component value. Of course, larger magnitudes will still use more bandwidth,
	// as has always been the case.
	switch (QuantizationLevel)
	{
	case EVectorQuantization::RoundTwoDecimals:
		return SerializePackedVector<100, 30>(Vector, Ar);
	case EVectorQuantization::RoundOneDecimal:
		return SerializePackedVector<10, 27>(Vector, Ar);
	default:
		return SerializePackedVector<1, 24>(Vector, Ar);

	}
}

void SerializeQuantizedRotator(FArchive& Ar, FRotator& Rotator, ERotatorQuantization QuantizationLevel)
{
	switch (QuantizationLevel)
	{
	case ERotatorQuantization::ByteComponents:
		Rotator.SerializeCompressed(Ar);
		break;
	case ERotatorQuantization::ShortComponents:
		Rotator.SerializeCompressedShort(Ar);
		break;
	}
}
