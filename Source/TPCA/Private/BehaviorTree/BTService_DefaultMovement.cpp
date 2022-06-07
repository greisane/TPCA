// This source code is licensed under the MIT license found in the LICENSE file in the root directory of this source tree.

#include "BehaviorTree/BTService_DefaultMovement.h"

#include "AIController.h"
#include "GameFramework/ExtCharacter.h"
#include "GameFramework/ExtCharacterMovementComponent.h"

UBTService_DefaultMovement::UBTService_DefaultMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NodeName = "Set default movement";

	bNotifyTick = false;
	bTickIntervals = false;
	bNotifyBecomeRelevant = true;
	bNotifyCeaseRelevant = true;
}

void UBTService_DefaultMovement::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);

	TNodeInstanceMemory* MyMemory = (TNodeInstanceMemory*)NodeMemory;
	MyMemory->Reset();

	if (AAIController* MyController = OwnerComp.GetAIOwner())
	{
		if (AExtCharacter* MyCharacter = Cast<AExtCharacter>(MyController->GetPawn()))
		{
			UExtCharacterMovementComponent* MyCharacterMovement = MyCharacter->GetExtCharacterMovement();
			MyMemory->bWasCrouching = MyCharacterMovement->bWantsToCrouch;
			MyMemory->bWasWalkingInsteadOfRunning = MyCharacterMovement->bWantsToWalkInsteadOfRun;
			MyMemory->bWasSprinting = MyCharacterMovement->bWantsToSprint;
			MyMemory->bWasPerformingGenericAction = MyCharacterMovement->bWantsToPerformGenericAction;

			if (bCrouch && !MyMemory->bWasCrouching)
			{
				MyCharacter->Crouch();
			}
			else if (!bCrouch && MyMemory->bWasCrouching)
			{
				MyCharacter->UnCrouch();
			}

			if (bWalkInsteadOfRun && !MyMemory->bWasWalkingInsteadOfRunning)
			{
				MyCharacter->Walk();
			}
			else if (!bWalkInsteadOfRun && MyMemory->bWasWalkingInsteadOfRunning)
			{
				MyCharacter->UnWalk();
			}

			if (bSprint && !MyMemory->bWasSprinting)
			{
				MyCharacter->Sprint();
			}
			else if (!bSprint && MyMemory->bWasSprinting)
			{
				MyCharacter->UnSprint();
			}

			if (bPerformGenericAction && !MyMemory->bWasPerformingGenericAction)
			{
				MyCharacter->PerformGenericAction();
			}
			else if (!bPerformGenericAction && MyMemory->bWasPerformingGenericAction)
			{
				MyCharacter->UnPerformGenericAction();
			}
		}
	}
}

void UBTService_DefaultMovement::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnCeaseRelevant(OwnerComp, NodeMemory);

	TNodeInstanceMemory* MyMemory = (TNodeInstanceMemory*)NodeMemory;

	if (AAIController* MyController = OwnerComp.GetAIOwner())
	{
		if (AExtCharacter* MyCharacter = Cast<AExtCharacter>(MyController->GetPawn()))
		{
			if (MyMemory->bWasCrouching)
			{
				MyCharacter->Crouch();
			}
			else
			{
				MyCharacter->UnCrouch();
			}

			if (MyMemory->bWasWalkingInsteadOfRunning)
			{
				MyCharacter->Walk();
			}
			else
			{
				MyCharacter->UnWalk();
			}

			if (MyMemory->bWasSprinting)
			{
				MyCharacter->Sprint();
			}
			else
			{
				MyCharacter->UnSprint();
			}

			if (MyMemory->bWasPerformingGenericAction)
			{
				MyCharacter->PerformGenericAction();
			}
			else
			{
				MyCharacter->UnPerformGenericAction();
			}
		}
	}
}

FString UBTService_DefaultMovement::GetStaticDescription() const
{
	TArray<FString> Descs;

	if (bCrouch)
	{
		Descs.Add(TEXT("Crouch"));
	}
	if (bWalkInsteadOfRun)
	{
		Descs.Add(TEXT("Walk"));
	}
	if (bSprint)
	{
		Descs.Add(TEXT("Sprint"));
	}
	if (bPerformGenericAction)
	{
		Descs.Add(TEXT("Perform Action"));
	}
	if (Descs.Num() == 0)
	{
		Descs.Add(TEXT("None"));
	}

	return FString::Join(Descs, TEXT(", "));
}

#if WITH_EDITOR

FName UBTService_DefaultMovement::GetNodeIconName() const
{
	return FName("BTEditor.Graph.BTNode.Service.DefaultFocus.Icon");
}

#endif	// WITH_EDITOR
