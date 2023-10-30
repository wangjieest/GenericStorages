// Copyright GenericStorages, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

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

	// means that it will create on bothside, and has a statble name path for repliction
	Instanced = 0xF UMETA(Hidden),

	BothSide = ServerSide | ClientSide UMETA(Hidden),
};
}  // namespace EComponentDeferredMode

namespace EComponentDeferredMode
{
FORCEINLINE bool HasAnyFlags(uint8 Test, uint8 Flags)
{
	return (Test & Flags) != 0;
}
FORCEINLINE bool HasAllFlags(uint8 Test, uint8 Flags)
{
	return (Test & Flags) == Flags;
}
}  // namespace EComponentDeferredMode

namespace DeferredComponentRegistry
{
struct FRegClassData
{
	TSubclassOf<UActorComponent> RegClass;
	uint8 RegFlags;

	// AddUnique
	inline bool operator==(const FRegClassData& Ohter) const { return Ohter.RegClass == RegClass; }
};
using FRegClassDataArray = TArray<FRegClassData>;
}  // namespace DeferredComponentRegistry

UCLASS()
class GENERICSTORAGES_API UDeferredComponentRegistry final : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
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

	static uint8 GetMode(uint8 InFlags, TSubclassOf<UActorComponent> InClass);
	static void ModifyDeferredComponents(TSubclassOf<AActor> Class, TFunctionRef<void(DeferredComponentRegistry::FRegClassDataArray&)> Cb, bool bPersistent = false);
};
