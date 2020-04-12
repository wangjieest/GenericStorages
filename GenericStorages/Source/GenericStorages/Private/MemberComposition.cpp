// Copyright 2018-2020 wangjieest, Inc. All Rights Reserved.

#include "MemberComposition.h"

#include "Engine/Engine.h"
#include "Engine/GameEngine.h"
#include "Engine/World.h"
#include "GenericSingletons.h"
#include "UObject/UObjectThreadContext.h"

namespace MemberComposition
{
static UMemberCompositionStorage* GetDataStorage(const UObject* WorldContextObj)
{
	return UGenericSingletons::GetSingleton<UMemberCompositionStorage>(WorldContextObj);
}

static TMap<FWeakObjectPtr, TArray<TWeakObjectPtr<UScriptStruct>>> Structs;

static void Remove(FWeakObjectPtr WeakObject)
{
	if (auto Storage = UGenericSingletons::GetSingleton<UMemberCompositionStorage>(WeakObject.Get(), false))
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
}  // namespace MemberComposition

FMemberCompositionDataBase::FMemberCompositionDataBase()
{
	FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
	UObject* Initializer = ThreadContext.IsInConstructor ? ThreadContext.TopInitializer()->GetObj() : nullptr;
	if (!Outer.IsValid())
		Outer = Initializer;
}

FMemberCompositionDataBase::~FMemberCompositionDataBase()
{
	if (Outer.IsStale())
	{
		MemberComposition::Remove(Outer);
	}
}

FMemberCompositionStorageType* FMemberComposition::GetStorageCell(UScriptStruct* Struct, const UObject* WorldContextObj)
{
	return MemberComposition::GetDataStorage(WorldContextObj)->DataStore.Find(Struct);
}

bool FMemberComposition::SetStorageCell(UScriptStruct* Struct, UObject* Object, FStructProperty* Prop, bool bReplaceExist)
{
	auto& Data = MemberComposition::GetDataStorage(Object)->DataStore.FindOrAdd(Struct);
	if (bReplaceExist || ensure(!Data.Obj || Data.Obj == Object))
	{
		Data.Obj = Object;
		Data.Prop = Prop;
		Prop->ContainerPtrToValuePtr<FMemberCompositionDataBase>(Object)->Outer = Object;
		return true;
	}
	return false;
}
