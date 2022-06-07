// This source code is licensed under the MIT license found in the LICENSE file in the root directory of this source tree.

#include "TPCA.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogTPCA)

#define LOCTEXT_NAMESPACE "TPCA"

class FTPCA : public IModuleInterface
{
	// Begin IModuleInterface Interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface Interface
};

IMPLEMENT_MODULE(FTPCA, TPCA);

void FTPCA::StartupModule()
{
	UE_LOG(LogTPCA, Log, TEXT("Third Person Character Actors (TPCA) Module Started"));
}

void FTPCA::ShutdownModule()
{
	UE_LOG(LogTPCA, Log, TEXT("Third Person Character Actors (TPCA) Module Shutdown"));
}

#undef LOCTEXT_NAMESPACE
