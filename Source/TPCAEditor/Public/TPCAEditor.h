// This source code is licensed under the MIT license found in the LICENSE file in the root directory of this source tree.

#pragma once

#include "CoreMinimal.h"
#include "UnrealEd.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorDelegates.h"
#include "IAssetTools.h"
#include "IAssetTypeActions.h"

class UAnimSequence;

DECLARE_LOG_CATEGORY_EXTERN(LogTPCAEditor, All, All)

class FTPCAEditor : public IModuleInterface
{
private:

	TArray<FName> RegisteredClassNames;
	TArray<FName> RegisteredPropertyTypes;
	TArray<FName> RegisteredComponentClassNames;
	TArray<TSharedRef<IAssetTypeActions>> RegisteredAssetTypeActions;

private:

	void RegisterCustomClassLayout(FName ClassName, FOnGetDetailCustomizationInstance DetailLayoutDelegate);
	void RegisterCustomPropertyTypeLayout(FName ClassName, FOnGetPropertyTypeCustomizationInstance DetailLayoutDelegate);

protected:

	void RegisterPropertyEditors();

public:

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
