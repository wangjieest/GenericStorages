// Copyright GenericStorages, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Engine/DataTable.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Info.h"
#include "GenericSingletons.h"
#include "Misc/MessageDialog.h"
#include "Templates/SubclassOf.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "GenericSystems.generated.h"

class UActorComponent;
class AGenericSystemBase;

UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = true))
enum class EDeferredFlag : uint8
{
	None UMETA(Hidden),
	CreateOnServer = 0x1,
	CreateOnClient = 0x2,
	SetReplicated = 0x4,
	SetNameStable = 0x8 UMETA(Hidden),
};

USTRUCT(BlueprintType)
struct GENERICSTORAGES_API FSystemDeferredCompoent
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "SystemDependcy", meta = (AllowAbstract = "true", AllowNone = "false"))
	TSubclassOf<AActor> ActorClass;

	UPROPERTY(EditAnywhere, Category = "SystemDependcy")
	TSubclassOf<UActorComponent> DeferredComponent;

	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "SystemDependcy", meta = (Bitmask, BitmaskEnum = "EDeferredFlag"))
	uint8 DeferredFlag = 0x3;

	bool operator==(const FSystemDeferredCompoent& Other) const { return ActorClass == Other.ActorClass && DeferredComponent == Other.DeferredComponent; }
};

USTRUCT(BlueprintType, BlueprintInternalUseOnly)
struct GENERICSTORAGES_API FGenericSystemConfig
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "GenericSystems")
	TSubclassOf<AGenericSystemBase> SystemClass;

	TWeakObjectPtr<AGenericSystemBase> SystemIns;

	bool operator==(const FGenericSystemConfig& rhs) const;
	bool operator==(const UClass* rhs) const;
};

UCLASS(Transient, hidedropdown, hideCategories = (ActorTick, Rendering, Replication, Collision, Input, LOD, Cooking))
class GENERICSTORAGES_API AGenericSystemMgr : public AActor
{
	GENERATED_BODY()
public:
	static AGenericSystemMgr* Get(class ULevel* Level, bool bCreate = true);
	AGenericSystemMgr();

#if WITH_EDITOR
	void AddSystemInstance(AGenericSystemBase* Ins);
	void DelSystemInstance(AGenericSystemBase* Ins);
#endif

protected:
	UPROPERTY(EditInstanceOnly, Category = "GenericSystems", meta = (ShowOnlyInnerProperties, NoElementDuplicate))
	TArray<FGenericSystemConfig> SystemSettings;

	virtual bool IsEditorOnly() const override { return true; }
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;

	UFUNCTION(CallInEditor, Category = "GenericSystems")
	void Refresh();
	void RefreshImpl(uint32 ChangeType, bool bNotify = false);

	UFUNCTION(CallInEditor, Category = "GenericSystems")
	void ClearUseless();

	virtual void PostLoad() override;
	virtual void PostActorCreated() override;
#endif
};

UCLASS(Abstract, hideCategories = (ActorTick, Rendering, Collision, Input, LOD, Cooking))
class GENERICSTORAGES_API AGenericSystemBase : public AInfo
{
	GENERATED_BODY()

public:
	AGenericSystemBase();

protected:
	enum class ESystemSource : uint8
	{
		LevelLoad,
		Duplicate,
		DynamicSpawn,
	};

	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void PostLoad() override;
	virtual void PostActorCreated() override;
	virtual void PostInitProperties() override;
	virtual bool IsNameStableForNetworking() const override;
	virtual void Destroyed() override;

protected:
	virtual void PostRegistered() {}
	virtual void RegisterToWorld(ESystemSource Src);

	UPROPERTY(EditAnywhere, Category = "GenericSystems", meta = (ShowOnlyInnerProperties))
	TArray<FSystemDeferredCompoent> DeferredComponents;
};
