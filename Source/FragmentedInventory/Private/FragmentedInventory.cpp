// Copyright Epic Games, Inc. All Rights Reserved.

#include "FragmentedInventory.h"

#include "Data/ItemDefinitionAsset.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Misc/CoreDelegates.h"

DEFINE_LOG_CATEGORY(LogFragmentedInventory)

void FFragmentedInventoryModule::StartupModule()
{
	if (GEngine != nullptr)
	{
		RegisterPrimaryAssets();
		return;
	}

	PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddRaw(this, &FFragmentedInventoryModule::HandlePostEngineInit);
}

void FFragmentedInventoryModule::ShutdownModule()
{
	if (PostEngineInitHandle.IsValid())
	{
		FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);
		PostEngineInitHandle.Reset();
	}
}

void FFragmentedInventoryModule::HandlePostEngineInit()
{
	PostEngineInitHandle.Reset();
	RegisterPrimaryAssets();
}

void FFragmentedInventoryModule::RegisterPrimaryAssets()
{
	const TArray<FString> ItemDefinitionAssetScanPaths { TEXT("/Game"), TEXT("/FragmentedInventory") };
	UAssetManager& AssetManager = UAssetManager::Get();
	AssetManager.ScanPathsForPrimaryAssets(
		UItemDefinitionAsset::PrimaryAssetType,
		ItemDefinitionAssetScanPaths,
		UItemDefinitionAsset::StaticClass(),
		false,
		false);
}

IMPLEMENT_MODULE(FFragmentedInventoryModule, FragmentedInventory)
