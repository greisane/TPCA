// This source code is licensed under the MIT license found in the LICENSE file in the root directory of this source tree.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/BTService.h"

#include "BTService_DefaultMovement.generated.h"

struct FBTMovementServiceMemory
{
	bool bWasCrouching;
	bool bWasWalkingInsteadOfRunning;
	bool bWasSprinting;
	bool bWasPerformingGenericAction;

	void Reset()
	{
		bWasCrouching = bWasWalkingInsteadOfRunning = bWasSprinting = bWasPerformingGenericAction = false;
	}
};

/**
 * Default Movement service node.
 * A service node that automatically sets an ExtCharacter's movement states when it becomes active.
 */
UCLASS(hidecategories=(Service))
class TPCA_API UBTService_DefaultMovement : public UBTService
{
	GENERATED_UCLASS_BODY()

	typedef FBTMovementServiceMemory TNodeInstanceMemory;

	/** Try to crouch, or keep crouching. */
	UPROPERTY(EditAnywhere, Category="Node")
	bool bCrouch;

	/** Try to walk instead of run, or keep walking. */
	UPROPERTY(EditAnywhere, Category="Node")
	bool bWalkInsteadOfRun;

	/** Try to sprint, or keep sprinting. */
	UPROPERTY(EditAnywhere, Category="Node")
	bool bSprint;

	/** Try to perform the generic action, or keep performing it. */
	UPROPERTY(EditAnywhere, Category="Node")
	bool bPerformGenericAction;

	virtual uint16 GetInstanceMemorySize() const override { return sizeof(TNodeInstanceMemory); }
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
#endif // WITH_EDITOR
};
