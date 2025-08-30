// Copyright GenericStorages, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "UnrealCompatibility.h"
#include "StaticProperty.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/UnrealTypePrivate.h"
#if UE_5_05_OR_LATER
#include "StructUtils/InstancedStruct.h"
#else
#include "InstancedStruct.h"
#endif
#include "SubSystemStorages.h"

#include "StaticPropertyPrivate.generated.h"

UCLASS(Transient)
class UStaticPropertiesContainer final : public UStruct
{
	GENERATED_BODY()
public:
	FName FindPropertyName(const FProperty* Property);
	virtual bool CanBeClusterRoot() const override { return true; }

	virtual void AddCppProperty(FProperty* Property) override;

	TMap<FProperty*, FName> PropLookups;
	TMap<FName, const FProperty*> NameLookups;
};

struct FSubsystemStorageUtils
{
	template<typename U>
	static void Clearup(U* This)
	{
		This->StructStores.Empty();
		This->ObjectStores.Empty();
		This->PropertyStores.Empty();
	}

	template<typename U>
	static bool SetKeyValue(U* This, FName KeyName, const FProperty* Prop, const void* Addr, EScopeSharedStorageOp Op = EScopeSharedStorageOp::TrySet)
	{
		if (auto* StructProp = CastField<FStructProperty>(Prop))
		{
			if (Addr == nullptr || Op == EScopeSharedStorageOp::Clear)
			{
				return !!This->StructStores.Remove(KeyName);
			}

			auto& Ref = This->StructStores.FindOrAdd(KeyName);
			if (Op == EScopeSharedStorageOp::Override || !Ref.IsValid())
			{
				Ref.InitializeAs(StructProp->Struct, static_cast<const uint8*>(Addr));
				return true;
			}
		}
		else if (auto* ObjProp = CastField<FObjectProperty>(Prop))
		{
			if (Addr == nullptr || Op == EScopeSharedStorageOp::Clear)
			{
				return !!This->ObjectStores.Remove(KeyName);
			}
			auto& Ref = This->ObjectStores.FindOrAdd(KeyName);
			if (Op == EScopeSharedStorageOp::Override || !Ref.Get())
			{
				Ref = static_cast<const UObject*>(Addr);
				return true;
			}
		}
		
		if (Addr == nullptr || Op == EScopeSharedStorageOp::Clear)
		{
			return !!This->PropertyStores.Remove(KeyName);
		}
		auto Find = This->PropertyStores.Find(KeyName);
		if (Op == EScopeSharedStorageOp::Override || !Find || !Find->IsValid())
		{
			if (!Find)
			{
				This->PropertyStores.Emplace(KeyName, FInstancedPropertyVal::MakeDuplicate(Prop, Addr));
			}
			else
			{
				*Find = FInstancedPropertyVal::MakeDuplicate(Prop, Addr);
			}
			return true;
		}
		return false;
	}

	template<typename U>
	static const void* GetValue(U* This, FName KeyName, const FProperty* Prop)
	{
		if (auto* StructProp = CastField<FStructProperty>(Prop))
		{
			if (auto Find = This->StructStores.Find(KeyName))
			{
				return Find->GetMemory();
			}
		}
		else if (auto* ObjProp = CastField<FObjectProperty>(Prop))
		{
			if (auto Find = This->ObjectStores.Find(KeyName))
			{
				return Find->Get();
			}
		}
		else if (auto Find = This->PropertyStores.Find(KeyName))
		{
			return (*Find)->GetMemory();
		}
		return nullptr;
	}
};

UCLASS()
class UPlayerSubSystemStorages : public ULocalPlayerSubsystem
{
	GENERATED_BODY()
protected:
	friend struct FSubsystemStorageUtils;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override {}
	virtual void Deinitialize() override
	{
		FSubsystemStorageUtils::Clearup(this);
		Super::Deinitialize();
	}

	UPROPERTY(Transient)
	TMap<FName, FInstancedStruct> StructStores;
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<const UObject>> ObjectStores;

	TMap<FName, FInstancedPropertyValPtr> PropertyStores;
};

UCLASS()
class UGameInstanceSubSystemStorages : public UGameInstanceSubsystem
{
	GENERATED_BODY()

protected:
	friend struct FSubsystemStorageUtils;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override {}
	virtual void Deinitialize() override
	{
		FSubsystemStorageUtils::Clearup(this);
		Super::Deinitialize();
	}

	UPROPERTY(Transient)
	TMap<FName, FInstancedStruct> StructStores;
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<const UObject>> ObjectStores;

	TMap<FName, FInstancedPropertyValPtr> PropertyStores;
};

UCLASS()
class UWorldSubSystemStorages : public UWorldSubsystem
{
	GENERATED_BODY()

protected:
	friend struct FSubsystemStorageUtils;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override {}
	virtual void Deinitialize() override
	{
		FSubsystemStorageUtils::Clearup(this);
		Super::Deinitialize();
	}
	UPROPERTY(Transient)
	TMap<FName, FInstancedStruct> StructStores;
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<const UObject>> ObjectStores;

	TMap<FName, FInstancedPropertyValPtr> PropertyStores;
};
