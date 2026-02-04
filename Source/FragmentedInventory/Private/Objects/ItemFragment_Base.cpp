// Fill out your copyright notice in the Description page of Project Settings.


#include "Objects/ItemFragment_Base.h"

UItemFragment_Base::UItemFragment_Base(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	FragmentDisplayName = FText::FromString(TEXT("Base Fragment"));
}

void UItemFragment_Base::InitializeDynamicData(FInstancedStruct& OutDynamicData) const
{
	const UScriptStruct* structType = GetDynamicDataStructType();
	if (!IsValid(structType))
	{
		UE_LOG(LogTemp, Error, TEXT("%hs:%d - GetDynamicDataStructType returned null for fragment %s"), __FUNCTION__, __LINE__, *GetNameSafe(this));
		return;
	}

	OutDynamicData.InitializeAs(structType);
}
