#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(GBufferProcessLog, Log, All);

class FGBufferProcessPlugin : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;

	virtual void ShutdownModule() override;

public:

	static inline FGBufferProcessPlugin& Get() {
		return FModuleManager::LoadModuleChecked< FGBufferProcessPlugin >("GBufferProcessPlugin");
	}

	static inline bool IsAvailable() {
		return FModuleManager::Get().IsModuleLoaded("GBufferProcessPlugin");
	}
};
