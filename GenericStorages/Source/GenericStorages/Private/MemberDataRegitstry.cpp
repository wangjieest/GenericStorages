// Copyright 2018-2020 wangjieest, Inc. All Rights Reserved.

#include "MemberDataRegitstry.h"

#include "Engine/Engine.h"
#include "Engine/GameEngine.h"
#include "Engine/World.h"
#include "GenericSingletons.h"
#include "UObject/UObjectThreadContext.h"

namespace MemberDataRegitstry
{
static UMemberDataRegistryStorage* GetDataStorage(const UObject* WorldContextObj)
{
	return UGenericSingletons::GetSingleton<UMemberDataRegistryStorage>(WorldContextObj);
}

static TMap<FWeakObjectPtr, TArray<TWeakObjectPtr<UScriptStruct>>> Structs;

static void Remove(FWeakObjectPtr WeakObject)
{
	if (auto Storage = UGenericSingletons::GetSingleton<UMemberDataRegistryStorage>(WeakObject.Get(), false))
	{
		TArray<TWeakObjectPtr<UScriptStruct>> Arr;
		if (Structs.RemoveAndCopyValue(WeakObject, Arr))
		{
			for (auto& a : Arr)
			{
				if (a.IsValid())
					Storage->DataStore.Remove(a.Get());
			}
		}
	}
}
}  // namespace MemberDataRegitstry

FMemberDataStorageBase::FMemberDataStorageBase()
{
	FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
	UObject* Initializer = ThreadContext.IsInConstructor ? ThreadContext.TopInitializer()->GetObj() : nullptr;
	if (!Outer.IsValid())
		Outer = Initializer;
}

FMemberDataStorageBase::~FMemberDataStorageBase()
{
	if (Outer.IsStale())
	{
		MemberDataRegitstry::Remove(Outer);
	}
}

FMemberDataRegistryType* FMemberDataRegitstry::GetStorageCell(UScriptStruct* Struct, const UObject* WorldContextObj)
{
	return MemberDataRegitstry::GetDataStorage(WorldContextObj)->DataStore.Find(Struct);
}

bool FMemberDataRegitstry::SetStorageCell(UScriptStruct* Struct, UObject* Object, FStructProperty* Prop, bool bReplaceExist)
{
	auto& Data = MemberDataRegitstry::GetDataStorage(Object)->DataStore.FindOrAdd(Struct);
	if (bReplaceExist || ensure(!Data.Obj || Data.Obj == Object))
	{
		Data.Obj = Object;
		Data.Prop = Prop;
		Prop->ContainerPtrToValuePtr<FMemberDataStorageBase>(Object)->Outer = Object;
		return true;
	}
	return false;
}
