/****************************************************************************
Copyright (c) 2017-2027 GenericStorages

author: wangjieest

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/

#pragma once

#include "CoreMinimal.h"

#include "AI/NavigationSystemBase.h"
#include "Engine/World.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Templates/SubclassOf.h"

#include "ComponentBookKeeper.generated.h"

class AActor;
class UActorComponent;
class APlayerController;

namespace EAttachedType
{
enum Flag
{
	ServerSide = 0x1,
	ClientSide = 0x2,
	Replicated = 0x4,

	NameStable = 0x8,
	Instanced = 0xF,
	SystemInstanced = 0xF UMETA(Hidden),
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

UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = true))
namespace ECompCreateMode
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
}  // namespace ECompCreateMode

UCLASS()
class GENERICSTORAGES_API UComponentBookKeeper final : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UComponentBookKeeper();
	UFUNCTION(BlueprintCallable, Category = "Game", meta = (CallableWithoutWorldContext = true))
	static void RegisterAutoSpawnComponents(TSubclassOf<AActor> Class,
											const TSet<TSubclassOf<UActorComponent>>& RegDatas,
											bool bPersistent = true,
											UPARAM(DisplayName = "CreateMode", meta = (Bitmask, BitmaskEnum = "ECompCreateMode")) uint8 Mode = 0);

	UFUNCTION(BlueprintCallable, Category = "Game", meta = (CallableWithoutWorldContext = true))
	static void RegisterAutoSpawnComponent(TSubclassOf<AActor> Class, TSubclassOf<UActorComponent> RegClass, bool bPersistent = true, UPARAM(DisplayName = "CreateMode", meta = (Bitmask, BitmaskEnum = "ECompCreateMode")) uint8 Mode = 0);

	UFUNCTION(BlueprintCallable, Category = "Game", meta = (CallableWithoutWorldContext = true))
	static void EnableAdd(bool bNewEnabled);
};
