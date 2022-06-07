// This source code is licensed under the MIT license found in the LICENSE file in the root directory of this source tree.

#include "Animation/ExtCharacterAnimInstance.h"
#include "Animation/AnimNode_StateMachine.h"
#include "Animation/BlendSpace.h"
#include "GameFramework/ExtCharacter.h"
#include "GameFramework/ExtCharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "Curves/CurveFloat.h"
#include "Math/MathExtensions.h"
#include "Kismet/Kismet.h"
#include "DrawDebugHelpers.h"
#include "TPCEMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogExtCharacterAnimInstance, Log, All);

const float UExtCharacterAnimInstance::AngleTolerance = 1e-3f;

UExtCharacterAnimInstance::UExtCharacterAnimInstance()
{
	AimOffsetInterpSpeed = 10.0f;
	AimOffsetResetInterpSpeed = 2.0f;
	AimOffsetMaxAngle = 0.0f;
	AimDistanceDefault = 200.0f;
	RootBoneResetSpeed = 180.0f;
	RootBoneResetCurveName = TEXT("RootBoneReset");

	WalkSpeed = 165.f;
	RunSpeed = 375.f;
	SprintSpeed = 600.f;

	WalkSpeedCrouched = 150.f;
	RunSpeedCrouched = 200.f;

	AnimWalkSpeed = 150.f;
	AnimRunSpeed = 375.f;
	AnimSprintSpeed = 600.f;

	AnimWalkSpeedCrouched = 150.f;
	AnimRunSpeedCrouched = 150.f;

	SlopeWalkSpeedScale = 1.0f;
	SlopeRunSpeedScale = 1.0f;

	GaitScale = 0.f;
	GaitScaleCrouched = 0.f;

	PlayRateWalk = 1.f;
	PlayRateWalkCrouched = 1.f;

	ForceVelocitySpeed = 5.f;
	ForceVelocityScale = 10.f;

	SpeedWarpScale = 1.0f;
}

void UExtCharacterAnimInstance::NativeInitializeAnimation()
{
	CharacterOwner = Cast<AExtCharacter>(TryGetPawnOwner());
	if (IsValid(CharacterOwner))
	{
		// Initial Character Position
		CharacterLocation = CharacterOwner->GetActorLocation();
		CharacterRotation = CharacterOwner->GetActorRotation();

		CharacterOwnerMovement = CharacterOwner->GetExtCharacterMovement();
		if (IsValid(CharacterOwnerMovement))
		{
			// Intial character state
			MovementMode = CharacterOwnerMovement->MovementMode;
			CustomMovementMode = CharacterOwnerMovement->CustomMovementMode;
		}

		Gait = CharacterOwner->GetGait();
		bIsCrouched = CharacterOwner->bIsCrouched;
		bIsPerformingGenericAction = CharacterOwner->bIsPerformingGenericAction;

		// Ragdoll Event Handler
		// Ensure delegate is bound (just once)
		CharacterOwner->RagdollChangedDelegate.RemoveDynamic(this, &UExtCharacterAnimInstance::HandleRagdollChanged);
		CharacterOwner->RagdollChangedDelegate.AddDynamic(this, &UExtCharacterAnimInstance::HandleRagdollChanged);

		CharacterOwnerMesh = GetSkelMeshComponent();
		if (IsValid(CharacterOwnerMesh))
		{
			LastCharacterMeshLocation = CharacterOwnerMesh->GetComponentLocation();
			RootBoneRotation = CharacterOwnerMesh->GetComponentQuat();
		}
	}
}


/// Every Tick

void UExtCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	if (IsValid(CharacterOwner)
		&& IsValid(CharacterOwnerMovement)
		&& IsValid(CharacterOwnerMesh)
		&& DeltaSeconds > 0.0f)
	{
		LastSpeed = Speed;
		LastGroundSpeed = GroundSpeed;

		const FTransform& CharacterMeshTransform = CharacterOwnerMesh->GetComponentTransform();
		const FVector CharacterMeshLocation = CharacterOwnerMesh->GetComponentLocation();
		const FVector CharacterMeshLocationDelta = (CharacterMeshLocation - LastCharacterMeshLocation).ProjectOnToNormal(CharacterOwnerMovement->Velocity.GetSafeNormal());
		LastCharacterMeshLocation = CharacterMeshLocation;

		const FVector LastVelocity = Velocity;
		// In order to reduce sliding in simulated proxies we use a Velocity calculated from the mesh displacement since last frame.
		Velocity = CharacterMeshLocationDelta / DeltaSeconds;
		Acceleration = CharacterOwnerMovement->GetCurrentAcceleration();
		bIsAccelerating = Acceleration.SizeSquared() > KINDA_SMALL_NUMBER;

		if (bIsAccelerating && ForceVelocitySpeed > 0.f)
		{
			// Using strong wind applying a constant force as an example...
			// If not accelerating, the character will walk in the direction of the wind
			// If accelerating into the wind, run even though the character more or less stays in place (getting pushed back)
			// If accelerating away from the wind, run faster than normal (pushed by the wind)
			FVector LastForceVelocity2D = CharacterOwnerMovement->LastForceVelocity;
			LastForceVelocity2D.Z = 0.f;
			SmoothForceVelocity = FMath::Lerp(SmoothForceVelocity, LastForceVelocity2D, DeltaSeconds * ForceVelocitySpeed);
			const float ForceVelocityWeight = FVector::DotProduct(SmoothForceVelocity.GetSafeNormal2D(), Acceleration.GetSafeNormal2D());
			Velocity += SmoothForceVelocity * (ForceVelocityScale * ForceVelocityWeight);
		}

		Speed = Velocity.Size();
		GroundSpeed = Velocity.Size2D();

		bWasMoving = bIsMoving;
		bWasMoving2D = bIsMoving2D;
		bIsMoving = Speed > 0.01f;
		bIsMoving2D = GroundSpeed > 0.01f;

		if (bIsMoving2D)
		{
			LastMovementVelocity = Velocity;
			LastMovementVelocityRotation = Velocity.Rotation();
		}

		LastMovementAcceleration = CharacterOwnerMovement->LastMovementAcceleration;
		LastMovementAccelerationRotation = LastMovementAcceleration.Rotation();

		bIsJumping = CharacterOwner->bIsJumping;

		bWasRagdoll = bIsRagdoll;
		bIsRagdoll = CharacterOwner->IsRagdoll();

		bWasGettingUp = bIsGettingUp;
		bIsGettingUp = CharacterOwner->IsGettingUp();

		RotationMode = CharacterOwner->GetRotationMode();

		LastCharacterLocation = CharacterLocation;
		LastCharacterRotation = CharacterRotation;

		CharacterLocation = CharacterOwner->GetActorLocation();
		CharacterRotation = CharacterOwner->GetActorRotation();

		// We have to recalculate drift (rather than using the one calculated by the character movement component)
		// because we use a velocity that is calculated out of mesh displacement
		const FRotator MeshOrientation = (RootBoneRotation * CharacterOwner->GetBaseRotationOffset().Inverse()).Rotator();
		MovementDrift = FMath::FindDeltaAngleDegrees(MeshOrientation.Yaw, LastMovementVelocityRotation.Yaw);

		LookRotation = CharacterOwner->GetLookRotation();
		LookDelta = (LookRotation - CharacterRotation).GetNormalized();
		LookAtActor = CharacterOwner->GetLookAtActor();

		GetUpDelay = CharacterOwner->GetUpDelay;

		UseHeadlook = CharacterOwner->UseHeadlook;
		UseBodylook = CharacterOwner->UseBodylook;
		bUseLookInputInMovement = CharacterOwner->bUseLookInputInMovement;

		SetMovementMode(CharacterOwnerMovement->MovementMode, CharacterOwnerMovement->CustomMovementMode);
		SetCrouched(CharacterOwner->bIsCrouched);
		SetGait(CharacterOwner->GetGait());
		SetPerformingGenericAction(CharacterOwner->bIsPerformingGenericAction);

		// Enable Foot IK only if enabled by the character, not ragdoll and moving on ground.
		bEnableFootIK = CharacterOwner->bEnableFootIK && !bIsRagdoll && (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking);
		bEnableLookIK = CharacterOwner->bEnableLookIK && !bIsRagdoll && (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking);

		if (bIsRagdoll)
		{
			if (MovementMode != MOVE_None && MovementMode != MOVE_Falling)
			{
				// Find if the ragdoll is facing up or down.
				const FQuat PelvisQuat = CharacterOwnerMesh->GetSocketQuaternion(CharacterOwner->GetPelvisBoneName());
				// Pelvis bone is assumed to be oriented Y-Fwd/X-Up so the right vector is the actual forward.
				bIsRagdollFacingDown = FVector::DotProduct(FVector::UpVector, PelvisQuat.GetRightVector()) < 0.0f;
				// In a ragdoll the capsule can rotate freely but we have to make sure the root bone is pointing in the right direction for the get up animation.
				// If the character is lying on its back the root bone must point to the feet but if the character is facing down the root bone must point to the head.
				RootBoneRotation = (bIsRagdollFacingDown ? FQuat(0.f, 0.f, -COS_45, COS_45) * PelvisQuat : FQuat(0.f, 0.f, COS_45, COS_45) * PelvisQuat);
				// Root bone is assumed to be oriented Y-Fwd/Z-Up so we have to fix the desired rotation by -90deg to align the Y-Axis to foward. Only then we can convert to component space.
				RootBoneOffset.X = CharacterOwnerMesh->GetComponentTransform().InverseTransformRotation(RootBoneRotation).Rotator().Yaw;
			}

			// Calculate IK bone locations for better blending out of ragdoll
			CharacterOwnerMesh->GetSocketWorldLocationAndRotation(CharacterOwner->GetLeftFootBoneName(), RagdollLeftFootLocation, RagdollLeftFootRotation);
			CharacterOwnerMesh->GetSocketWorldLocationAndRotation(CharacterOwner->GetRightFootBoneName(), RagdollRightFootLocation, RagdollRightFootRotation);

			// Reset Aim Offset
			TargetAimOffset = FVector2D(0.f, 0.f);
			AimOffset = TargetAimOffset;
			AimDistance = AimDistanceDefault;
		}
		else
		{
			if (RootBoneOffset.X < -AngleTolerance || RootBoneOffset.X > AngleTolerance)
			{
				const float RootBoneSpeedFactor = bIsGettingUp ? GetCurveValue(RootBoneResetCurveName) : 1.f;
				if (RootBoneSpeedFactor > 0.f)
				{
					RootBoneOffset.X = FMathEx::FInterpConstantAngleTo(RootBoneOffset.X, 0.0f, DeltaSeconds, RootBoneResetSpeed * RootBoneSpeedFactor);
					RootBoneRotation = CharacterOwnerMesh->GetComponentTransform().TransformRotation(FQuat(FVector::UpVector, FMath::DegreesToRadians(RootBoneOffset.X)));
				}
			}
			else
			{
				RootBoneOffset.X = 0.0f;
				RootBoneRotation = CharacterOwnerMesh->GetComponentQuat();
			}

			if (!bIsGettingUp)
			{
				NativeUpdateGaitScale(DeltaSeconds);
				NativeUpdatePivotTurn(LastVelocity, DeltaSeconds);
				NativeUpdateTurnInPlace(DeltaSeconds);
				NativeUpdateAimOffset(DeltaSeconds);
			}
		}

		RaiseEvents();
	}
}

void UExtCharacterAnimInstance::NativeUpdateGaitScale(float DeltaSeconds)
{
	// Calculate GaitScale, WalkPlayRate and SpeedWarp. Gait Scale is a value in the range [0, 3] where
	// 0 is fully stopped; 1 is fully walking; 2 is fully running; 3 is fully sprinting and values in between are blends.
	if (bIsMoving2D)
	{
		if (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking)
		{
			float NewSpeedWarpScale;
			float SlopeSpeedScale;
			if (bIsCrouched)
			{
				float AnimSpeedScale;
				if (GroundSpeed <= WalkSpeedCrouched)
				{
					GaitScaleCrouched = FMath::GetRangePct(FVector2D(0.f, WalkSpeedCrouched), GroundSpeed);
					AnimSpeedScale = GroundSpeed / AnimWalkSpeedCrouched;
					SlopeSpeedScale = SlopeWalkSpeedScale;
				}
				else if (GroundSpeed <= RunSpeedCrouched)
				{
					const float Alpha = FMath::GetRangePct(FVector2D(WalkSpeedCrouched, RunSpeedCrouched), GroundSpeed);
					GaitScaleCrouched = 1.0f + Alpha;
					AnimSpeedScale = GroundSpeed / FMath::Lerp(AnimWalkSpeedCrouched, AnimRunSpeedCrouched, Alpha);
					SlopeSpeedScale = SlopeRunSpeedScale;
				}
				else
				{
					GaitScaleCrouched = 2.0f;
					AnimSpeedScale = GroundSpeed / AnimRunSpeedCrouched;
					SlopeSpeedScale = SlopeRunSpeedScale;
				}

				if (AnimSpeedScale < 1.0f)
				{
					const float Deviation = AnimSpeedScale - 1.0f;
					const float PlayRateDeviation = FMath::Max(-0.15f, Deviation);
					const float SpeedWarpDeviation = FMath::Max(-0.85f, Deviation - PlayRateDeviation);

					PlayRateWalkCrouched = 1.0f + PlayRateDeviation;
					NewSpeedWarpScale = 1.0f + SpeedWarpDeviation;
				}
				else
				{
					PlayRateWalkCrouched = 0.2f * AnimSpeedScale + 0.8f;
					NewSpeedWarpScale = 0.8f * AnimSpeedScale + 0.2f;
				}
			}
			else
			{
				float AnimSpeedScale;
				if (GroundSpeed <= WalkSpeed)
				{
					GaitScale = FMath::GetRangePct(FVector2D(0.f, WalkSpeed), GroundSpeed);
					AnimSpeedScale = GroundSpeed / AnimWalkSpeed;
					SlopeSpeedScale = SlopeWalkSpeedScale;
				}
				else if (GroundSpeed <= RunSpeed)
				{
					const float Alpha = FMath::GetRangePct(FVector2D(WalkSpeed, RunSpeed), GroundSpeed);
					GaitScale = 1.0f + Alpha;
					AnimSpeedScale = GroundSpeed / FMath::Lerp(AnimWalkSpeed, AnimRunSpeed, Alpha);
					SlopeSpeedScale = SlopeRunSpeedScale;
				}
				else if (GroundSpeed <= SprintSpeed)
				{
					const float Alpha = FMath::GetRangePct(FVector2D(RunSpeed, SprintSpeed), GroundSpeed);
					GaitScale = 2.0f + Alpha;
					AnimSpeedScale = GroundSpeed / FMath::Lerp(AnimRunSpeed, AnimSprintSpeed, Alpha);
					SlopeSpeedScale = SlopeRunSpeedScale;
				}
				else
				{
					GaitScale = 3.0f;
					AnimSpeedScale = GroundSpeed / AnimSprintSpeed;
					SlopeSpeedScale = SlopeRunSpeedScale;
				}

				if (AnimSpeedScale < 1.0f)
				{
					const float Deviation = AnimSpeedScale - 1.0f;
					const float PlayRateDeviation = FMath::Max(-0.15f, Deviation);
					const float SpeedWarpDeviation = FMath::Max(-0.85f, Deviation - PlayRateDeviation);

					PlayRateWalk = 1.0f + PlayRateDeviation;
					NewSpeedWarpScale = 1.0f + SpeedWarpDeviation;
				}
				else
				{
					PlayRateWalk = 0.2f * AnimSpeedScale + 0.8f;
					NewSpeedWarpScale = 0.8f * AnimSpeedScale + 0.2f;
				}
			}

			// Apply slope speed scale
			const FHitResult& RampHit = CharacterOwnerMovement->CurrentFloor.HitResult;
			const FVector FloorNormal = RampHit.ImpactNormal;
			const FVector ContactNormal = RampHit.Normal;
			const bool bHitFromLineTrace = CharacterOwnerMovement->CurrentFloor.bLineTrace;

			if (FloorNormal.Z < (1.f - KINDA_SMALL_NUMBER)
				&& FloorNormal.Z > KINDA_SMALL_NUMBER
				&& ContactNormal.Z > KINDA_SMALL_NUMBER
				&& !bHitFromLineTrace
				&& CharacterOwnerMovement->IsWalkable(RampHit))
			{
				PlayRateWalk *= FMath::Lerp(SlopeSpeedScale, 1.f, FloorNormal.Z);
			}

			// Interpolation produces a little bit of foot sliding but improves leg/feet blending dramaticaly, specially when running.
			SpeedWarpScale = FMathEx::FSafeInterpTo(SpeedWarpScale, NewSpeedWarpScale, DeltaSeconds, FMath::GetMappedRangeValueUnclamped(FVector2D(1.f, 3.f), FVector2D(100.f, 10.f), GaitScale));
		}
	}
}

void UExtCharacterAnimInstance::NativeUpdatePivotTurn(const FVector& InLastVelocity, float DeltaSeconds)
{
	bool bIsPivotTurningInstantly = false;
	bWasPivotTurning = bIsPivotTurning;
	bIsPivotTurning = CharacterOwnerMovement->IsPivotTurning();

	if (!bIsPivotTurning)
	{
		bIsPivotTurningInstantly = FVector::DotProduct(InLastVelocity.GetSafeNormal2D(), Velocity.GetSafeNormal2D()) < -0.173648f;
	}

	if (bIsPivotTurningInstantly || (!bWasPivotTurning && bIsPivotTurning))
	{
		if (MovementDrift > 0.f)
		{
			if (MovementDrift < 50.f)
				PivotTurnDirection = ECardinalDirection::North;
			else if (MovementDrift > 130.f)
				PivotTurnDirection = ECardinalDirection::South;
			else
				PivotTurnDirection = ECardinalDirection::East;
		}
		else
		{
			if (MovementDrift > -50.f)
				PivotTurnDirection = ECardinalDirection::North;
			else if (MovementDrift < -130.f)
				PivotTurnDirection = ECardinalDirection::South;
			else
				PivotTurnDirection = ECardinalDirection::West;
		}
	}
}

void UExtCharacterAnimInstance::NativeUpdateTurnInPlace(float DeltaSeconds)
{
	bWasTurningInPlace = bIsTurningInPlace;
	bWasTurningInPlaceRight = bIsTurningInPlaceRight;

	const ETurnInPlaceState TurnInPlaceState = CharacterOwnerMovement->GetTurnInPlaceState();

	if (((bWasRagdoll && !bIsRagdoll) || bWasGettingUp) && !bIsMoving && MovementMode != MOVE_None && MovementMode != MOVE_Falling)
	{
		bIsTurningInPlace = true;
		TurnInPlaceTargetYaw = CharacterRotation.Yaw;
	}

	bIsTurningInPlace = (TurnInPlaceState == ETurnInPlaceState::InProgress) || (bIsTurningInPlace && (TurnInPlaceState == ETurnInPlaceState::Done));

	if (bIsTurningInPlace)
	{
		// Cached target yaw. Only valid when bWasTurningInPlace is true
		const float PreviousTurnInPlaceTargetYaw = TurnInPlaceTargetYaw;

		if (TurnInPlaceState == ETurnInPlaceState::InProgress)
			TurnInPlaceTargetYaw = CharacterOwnerMovement->GetTurnInPlaceTargetYaw();

		const float TurnInPlaceDelta = FMath::FindDeltaAngleDegrees(LastCharacterRotation.Yaw, CharacterRotation.Yaw);
		if (TurnInPlaceDelta < -AngleTolerance)
			bIsTurningInPlaceRight = false;
		else if (TurnInPlaceDelta > AngleTolerance)
			bIsTurningInPlaceRight = true;

		float TargetDeltaRemaining = TurnInPlaceTargetYaw - (RootBoneRotation * CharacterOwner->GetBaseRotationOffset().Inverse()).Rotator().Yaw;

		if (FMath::IsNearlyZero(FMath::UnwindDegrees(TargetDeltaRemaining), AngleTolerance))
			bIsTurningInPlace = false;
		else
		{
			if (bIsTurningInPlaceRight)
			{
				if (TargetDeltaRemaining < 0.f)
					TargetDeltaRemaining += 360.f;

				// If we just started turning find if this is a long or short turn and if it should finish long or not.
				// If the target yaw has been updated just check if we should still finish long. Once a turn becomes short it does not come back to long.
				if (!bWasTurningInPlace || !bWasTurningInPlaceRight)
				{
					bIsTurnInPlaceLong = TargetDeltaRemaining > 90.f;
					bShouldTurnInPlaceFinishLong = TargetDeltaRemaining < 180.f || FMath::IsNearlyZero(FMath::Fmod(TargetDeltaRemaining, 180.f), AngleTolerance);
				}
				else if (TurnInPlaceTargetYaw != PreviousTurnInPlaceTargetYaw)
				{
					bShouldTurnInPlaceFinishLong = bIsTurnInPlaceLong && FMath::IsNearlyZero(FMath::Fmod(TargetDeltaRemaining, 180.f), AngleTolerance);
				}

				if (bIsTurnInPlaceLong && TargetDeltaRemaining <= 90.f && !bShouldTurnInPlaceFinishLong)
				{
					bIsTurnInPlaceLong = false;
				}

				if (bIsCrouched)
				{
					bIsTurnInPlaceLong = false;
					TurnInPlaceRightAnimPositionCrouched = TurnInPlaceRightCurveCrouched ? TurnInPlaceRightCurveCrouched->GetFloatValue(FMath::Fmod(TargetDeltaRemaining, 90.0f)) : 0.f;
				}
				else if (bIsPerformingGenericAction)
				{
					bIsTurnInPlaceLong = false;
					TurnInPlaceRightAnimPositionLeftFootFwd = TurnInPlaceRightCurveLeftFootFwd ? TurnInPlaceRightCurveLeftFootFwd->GetFloatValue(FMath::Fmod(TargetDeltaRemaining, 90.0f)) : 0.f;
				}
				else
				{
					if (bIsTurnInPlaceLong)
						TurnInPlaceRightLongAnimPositionNormal = TurnInPlaceRightLongCurveNormal ? TurnInPlaceRightLongCurveNormal->GetFloatValue(FMath::Fmod(TargetDeltaRemaining, 180.0f)) : 0.f;
					else
						TurnInPlaceRightShortAnimPositionNormal = TurnInPlaceRightShortCurveNormal ? TurnInPlaceRightShortCurveNormal->GetFloatValue(FMath::Fmod(TargetDeltaRemaining, 90.0f)) : 0.f;
				}
			}
			else
			{
				if (TargetDeltaRemaining > 0.f)
					TargetDeltaRemaining -= 360.f;

				// If we just started turning, find if this is a long or short turn and if it should finish long or not.
				// If the target yaw has been updated just check if we should still finish long. Once a turn becomes short it does not come back to long.
				if (!bWasTurningInPlace || bWasTurningInPlaceRight)
				{
					bIsTurnInPlaceLong = TargetDeltaRemaining < -90.f;
					bShouldTurnInPlaceFinishLong = TargetDeltaRemaining > -180.f || FMath::IsNearlyZero(FMath::Fmod(TargetDeltaRemaining, 180.f), AngleTolerance);
				}
				else if (TurnInPlaceTargetYaw != PreviousTurnInPlaceTargetYaw)
				{
					bShouldTurnInPlaceFinishLong = bIsTurnInPlaceLong && FMath::IsNearlyZero(FMath::Fmod(TargetDeltaRemaining, 180.f), AngleTolerance);
				}

				if (bIsTurnInPlaceLong && TargetDeltaRemaining >= -90.f && !bShouldTurnInPlaceFinishLong)
				{
					bIsTurnInPlaceLong = false;
				}

				if (bIsCrouched)
				{
					bIsTurnInPlaceLong = false;
					TurnInPlaceLeftAnimPositionCrouched = TurnInPlaceLeftCurveCrouched ? TurnInPlaceLeftCurveCrouched->GetFloatValue(FMath::Fmod(TargetDeltaRemaining, 90.0f)) : 0.f;
				}
				else if (bIsPerformingGenericAction)
				{
					bIsTurnInPlaceLong = false;
					TurnInPlaceLeftAnimPositionLeftFootFwd = TurnInPlaceLeftCurveLeftFootFwd ? TurnInPlaceLeftCurveLeftFootFwd->GetFloatValue(FMath::Fmod(TargetDeltaRemaining, 90.0f)) : 0.f;
				}
				else
				{
					if (bIsTurnInPlaceLong)
						TurnInPlaceLeftLongAnimPositionNormal = TurnInPlaceLeftLongCurveNormal ? TurnInPlaceLeftLongCurveNormal->GetFloatValue(FMath::Fmod(TargetDeltaRemaining, 180.0f)) : 0.f;
					else
						TurnInPlaceLeftShortAnimPositionNormal = TurnInPlaceLeftShortCurveNormal ? TurnInPlaceLeftShortCurveNormal->GetFloatValue(FMath::Fmod(TargetDeltaRemaining, 90.0f)) : 0.f;
				}
			}
		}
	}
}

void UExtCharacterAnimInstance::NativeUpdateAimOffset(float DeltaSeconds)
{
	TargetAimOffset = FVector2D::ZeroVector;
	AimDistance = AimDistanceDefault;
	float InterpSpeed = AimOffsetResetInterpSpeed;

	if (MovementMode != MOVE_None && (MovementMode != MOVE_Falling || bIsJumping))
	{
		if (IsValid(LookAtActor))
		{
			// Look at target
			const FVector DeltaLoc = LookAtActor->GetTargetLocation(CharacterOwner) - CharacterOwner->GetPawnViewLocation();
			const FRotator Delta = (FRotationMatrix::MakeFromX(DeltaLoc).Rotator() - CharacterOwner->GetActorRotation()).GetNormalized();
			TargetAimOffset = FVector2D(Delta.Yaw, bIsJumping && Velocity.Z < 0.f ? Delta.Pitch - 60.f : Delta.Pitch);
			AimDistance = DeltaLoc.Size();
			InterpSpeed = AimOffsetInterpSpeed;
		}
		else if (RotationMode == ECharacterRotationMode::OrientToController)
		{
			// Use look rotation
			TargetAimOffset = FVector2D(LookDelta.Yaw, bIsJumping && Velocity.Z < 0.f ? LookDelta.Pitch - 60.f : LookDelta.Pitch);
			InterpSpeed = AimOffsetInterpSpeed;
		}
		else if (RotationMode == ECharacterRotationMode::OrientToMovement && bIsAccelerating)
		{
			// Look in the direction of input
			const FRotator Delta = (LastMovementAccelerationRotation - CharacterRotation).GetNormalized();
			TargetAimOffset = FVector2D(Delta.Yaw, bIsJumping && Velocity.Z < 0.f ? Delta.Pitch - 60.f : Delta.Pitch);
			InterpSpeed = AimOffsetInterpSpeed;
		}
		else if (RotationMode == ECharacterRotationMode::OrientToMovement && bIsMoving)
		{
			// Look in the direction of movement
			TargetAimOffset = FVector2D(MovementDrift, bIsJumping && Velocity.Z < 0.f ? LookDelta.Pitch - 60.f : LookDelta.Pitch);
			InterpSpeed = AimOffsetInterpSpeed;
		}

		if (RotationMode == ECharacterRotationMode::OrientToMovement && !bIsJumping && CharacterOwner->bUseLookInputInMovement)
		{
			// Use look rotation even if moving, as long as it's not too extreme
			if (FMath::FindDeltaAngleDegrees(LookDelta.Yaw, TargetAimOffset.X) <= 90.f)
			{
				TargetAimOffset = FVector2D(LookDelta.Yaw, LookDelta.Pitch);
			}
		}
	}

	AimOffset = FMathEx::Vector2DSafeInterpTo(AimOffset, TargetAimOffset, DeltaSeconds, InterpSpeed);
	if (AimOffsetMaxAngle > 0.0f)
	{
		AimOffset = AimOffset.ClampAxes(-AimOffsetMaxAngle, AimOffsetMaxAngle);
	}

	AimLocation = UKismetMathLibrary::CreateVectorFromYawPitch(AimOffset.X, AimOffset.Y, AimDistance);
	AimLocation = CharacterOwner->GetActorRotation().RotateVector(AimLocation) + CharacterOwner->GetPawnViewLocation();
}

void UExtCharacterAnimInstance::RaiseEvents()
{
	if (bHasMovementModeChanged)
	{
		bHasMovementModeChanged = false;

		if (!bIsRagdoll)
			StopAllMontages(0.1f);

		OnMovementModeChanged();
	}

	if (bHasGaitChanged)
	{
		bHasGaitChanged = false;
		OnGaitChanged();
	}

	if (bHasCrouchedChanged)
	{
		bHasCrouchedChanged = false;
		OnCrouchedChanged();
	}

	if (bHasPerformingGenericActionChanged)
	{
		bHasPerformingGenericActionChanged = false;
		OnPerformingGenericActionChanged();
	}
}


/// Setters

void UExtCharacterAnimInstance::SetMovementMode(const EMovementMode Value, const uint8 CustomValue)
{
	if (MovementMode != Value || (Value == MOVE_Custom && CustomMovementMode != CustomValue))
	{
		MovementMode = Value;
		CustomMovementMode = CustomValue;
		bHasMovementModeChanged = true;
	}
}

void UExtCharacterAnimInstance::SetCrouched(const bool Value)
{
	if (bIsCrouched != Value)
	{
		bIsCrouched = Value;
		bHasCrouchedChanged = true;
	}
}

void UExtCharacterAnimInstance::SetGait(const ECharacterGait Value)
{
	if (Gait != Value)
	{
		Gait = Value;
		bHasGaitChanged = true;
	}
}

void UExtCharacterAnimInstance::SetPerformingGenericAction(const bool Value)
{
	if (bIsPerformingGenericAction != Value)
	{
		bIsPerformingGenericAction = Value;
		bHasPerformingGenericActionChanged = true;
	}
}


/// Handlers

void UExtCharacterAnimInstance::HandleRagdollChanged(AExtCharacter* Sender)
{
	if (!Sender->IsRagdoll())
		OnRagdollEnded();
}


/// Curve Utilities

float UExtCharacterAnimInstance::FindCurveTimeFromValue(UAnimSequence* InAnimSequence, const FName CurveName, const float Value) const
{
	if (InAnimSequence)
		for (const FFloatCurve& Curve : InAnimSequence->GetCurveData().FloatCurves)
			if (Curve.Name.DisplayName == CurveName)
			{
				const TArray<FRichCurveKey>& Keys = Curve.FloatCurve.GetConstRefOfKeys();

				const int32 NumKeys = Keys.Num();
				if (NumKeys < 2)
				{
					return 0.f;
				}

				// Some assumptions:
				// - keys have unique values, so for a given value, it maps to a single position on the timeline of the animation.
				// - key values are sorted in increasing order.

#ifdef UE_BUILD_DEBUG
				// verify assumptions in DEBUG
				bool bIsSortedInIncreasingOrder = true;
				bool bHasUniqueValues = true;
				TMap<float, float> UniquenessMap;
				UniquenessMap.Add(Keys[0].Value, Keys[0].Time);
				for (int32 KeyIndex = 1; KeyIndex < Keys.Num(); KeyIndex++)
				{
					if (UniquenessMap.Find(Keys[KeyIndex].Value) != nullptr)
					{
						bHasUniqueValues = false;
					}

					UniquenessMap.Add(Keys[KeyIndex].Value, Keys[KeyIndex].Time);

					if (Keys[KeyIndex].Value < Keys[KeyIndex - 1].Value)
					{
						bIsSortedInIncreasingOrder = false;
					}
				}

				if (!bIsSortedInIncreasingOrder || !bHasUniqueValues)
				{
					UE_LOG(LogExtCharacterAnimInstance, Warning, TEXT("ERROR: BAD DISTANCE CURVE: %s, bIsSortedInIncreasingOrder: %d, bHasUniqueValues: %d"), *GetNameSafe(InAnimSequence), bIsSortedInIncreasingOrder, bHasUniqueValues);
				}
#endif

				int32 first = 1;
				int32 last = NumKeys - 1;
				int32 count = last - first;

				while (count > 0)
				{
					int32 step = count / 2;
					int32 middle = first + step;

					if (Value > Keys[middle].Value)
					{
						first = middle + 1;
						count -= step + 1;
					}
					else
					{
						count = step;
					}
				}

				const FRichCurveKey& KeyA = Keys[first - 1];
				const FRichCurveKey& KeyB = Keys[first];
				const float Diff = KeyB.Value - KeyA.Value;
				const float Alpha = !FMath::IsNearlyZero(Diff) ? ((Value - KeyA.Value) / Diff) : 0.f;
				return FMath::Lerp(KeyA.Time, KeyB.Time, Alpha);
			}

	return 0.0f;
}

