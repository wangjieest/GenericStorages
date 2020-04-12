// Copyright 2018-2020 wangjieest, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "AI/NavigationSystemBase.h"
#include "Engine/World.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Templates/SubclassOf.h"

#include "DeferredComponentRegistry.generated.h"

class AActor;
class UActorComponent;
class APlayerController;

UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = true))
namespace EComponentDeferredMode
{
enum Type
{
	None,
	ServerSide = 0x1 UMETA(DisplayName = "CreateOnServer", ToolTip = "CreateOnServer"),
	ClientSide = 0x2 UMETA(DisplayName = "CreateOnClient", ToolTip = "CreateOnClient"),
	Replicated = 0x4 UMETA(DisplayName = "SetReplicate", ToolTip = "SetReplicate"),
	NameStable = 0x8 UMETA(DisplayName = "NameStable", ToolTip = "Component,NameStable"),

	BothSide = 0x3 UMETA(Hidden),
};
}  // namespace EComponentDeferredMode

namespace EAttachedType
{
enum Flag
{
	ServerSide = 0x1,
	ClientSide = 0x2,
	Replicated = 0x4,
	NameStable = 0x8,

	// means that it will create on bothside, and has a statble name path for repliction
	Instanced = 0xF,

	BothSide = 0x3,
};

FORCEINLINE bool HasAnyFlags(uint8 Test, uint8 Flags)
{
	return (Test & Flags) != 0;
}
FORCEINLINE bool HasAllFlags(uint8 Test, uint8 Flags)
{
	return (Test & Flags) == Flags;
}
}  // namespace EAttachedType

UCLASS()
class GENERICSTORAGES_API UDeferredComponentRegistry final : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UDeferredComponentRegistry();
	UFUNCTION(BlueprintCallable, Category = "Game", meta = (CallableWithoutWorldContext = true))
	static void AddDeferredComponents(TSubclassOf<AActor> Class,
									  const TSet<TSubclassOf<UActorComponent>>& RegDatas,
									  bool bPersistent = true,
									  UPARAM(DisplayName = "CreateMode", meta = (Bitmask, BitmaskEnum = "EComponentDeferredMode")) uint8 Mode = 0);

	UFUNCTION(BlueprintCallable, Category = "Game", meta = (CallableWithoutWorldContext = true))
	static void AddDeferredComponent(TSubclassOf<AActor> Class,
									 TSubclassOf<UActorComponent> ComponentClass,
									 bool bPersistent = true,
									 UPARAM(DisplayName = "CreateMode", meta = (Bitmask, BitmaskEnum = "EComponentDeferredMode")) uint8 Mode = 0);

	UFUNCTION(BlueprintCallable, Category = "Game", meta = (CallableWithoutWorldContext = true))
	static void EnableAdd(bool bNewEnabled);
};
