// This source code is licensed under the MIT license found in the LICENSE file in the root directory of this source tree.

#include "TPCAEditor.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ExtCharacter.h"
#include "GameFramework/ExtCharacterMovementComponent.h"

#include "DetailCustomizations/CharacterDetails.h"
#include "DetailCustomizations/CharacterMovementDetails.h"
#include "DetailCustomizations/ExtCharacterDetails.h"
#include "DetailCustomizations/ExtCharacterMovementDetails.h"

#include "Editor.h"

DEFINE_LOG_CATEGORY(LogTPCAEditor)

#define LOCTEXT_NAMESPACE "TPCAEditor"

IMPLEMENT_MODULE(FTPCAEditor, TPCAEditor);

void FTPCAEditor::StartupModule()
{
	RegisterPropertyEditors();

	UE_LOG(LogTPCAEditor, Log, TEXT("Third Person Character Actors (TPCA) Editor Module Started"));
}

void FTPCAEditor::ShutdownModule()
{
	// Unregister property editors
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

		// Unregister all classes customized by name
		for (auto It = RegisteredClassNames.CreateConstIterator(); It; ++It)
		{
			if (It->IsValid())
			{
				PropertyModule.UnregisterCustomClassLayout(*It);
			}
		}

		// Unregister all structures
		for (auto It = RegisteredPropertyTypes.CreateConstIterator(); It; ++It)
		{
			if (It->IsValid())
			{
				PropertyModule.UnregisterCustomPropertyTypeLayout(*It);
			}
		}
	}
	else
	{
		UE_LOG(LogTPCAEditor, Error, TEXT("Cannot unregister types: PropertyEditor not loaded."));
	}

	// Unregister component visualizers
	if (GUnrealEd)
	{
		for (FName& ClassName : RegisteredComponentClassNames)
		{
			GUnrealEd->UnregisterComponentVisualizer(ClassName);
		}
	}
	else
	{
//Error due to GUnrealEd being NULL causes packaging failure
#if !UE_BUILD_SHIPPING && !UE_BUILD_DEVELOPMENT
		UE_LOG(LogTPCAEditor, Error, TEXT("Cannot unregister component visualizers: GUnrealEd is null."));
#endif
	}

    UE_LOG(LogTPCAEditor, Log, TEXT("Third Person Character Actors (TPCA) Editor Module Shutdown"));
}

void FTPCAEditor::RegisterPropertyEditors()
{
	RegisterCustomClassLayout(ACharacter::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FCharacterDetails::MakeInstance));
	RegisterCustomClassLayout(AExtCharacter::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FExtCharacterDetails::MakeInstance));

	RegisterCustomClassLayout(UCharacterMovementComponent::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FCharacterMovementDetails::MakeInstance));
	RegisterCustomClassLayout(UExtCharacterMovementComponent::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FExtCharacterMovementDetails::MakeInstance));
}

void FTPCAEditor::RegisterCustomClassLayout(FName ClassName, FOnGetDetailCustomizationInstance DetailLayoutDelegate)
{
	check(ClassName != NAME_None);

	RegisteredClassNames.Add(ClassName);

	static const FName PropertyEditor("PropertyEditor");
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditor);
	PropertyModule.RegisterCustomClassLayout(ClassName, DetailLayoutDelegate);
}

void FTPCAEditor::RegisterCustomPropertyTypeLayout(FName PropertyTypeName, FOnGetPropertyTypeCustomizationInstance PropertyTypeLayoutDelegate)
{
	check(PropertyTypeName != NAME_None);

	RegisteredPropertyTypes.Add(PropertyTypeName);

	static const FName PropertyEditor("PropertyEditor");
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditor);
	PropertyModule.RegisterCustomPropertyTypeLayout(PropertyTypeName, PropertyTypeLayoutDelegate);
}

#undef LOCTEXT_NAMESPACE
