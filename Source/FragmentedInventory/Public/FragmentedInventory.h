// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogFragmentedInventory, Log, All);

class FRAGMENTEDINVENTORY_API FFragmentedInventoryModule : public IModuleInterface
{
public:
	/** @brief Registers runtime inventory assets. */
	virtual void StartupModule() override;

	/** @brief Releases startup delegates. */
	virtual void ShutdownModule() override;

private:
	/** @brief Registers item definition primary assets after engine initialization. */
	void RegisterPrimaryAssets();

	/** @brief Deferred engine startup callback. */
	void HandlePostEngineInit();

	FDelegateHandle PostEngineInitHandle;
};
