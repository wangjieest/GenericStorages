// Copyright 2018-2020 wangjieest, Inc. All Rights Reserved.

#include "ObjectRegistry.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GenericSingletons.h"
#include "UnrealCompatibility.h"

FCriticalSection UObjectRegistry::Critical;

UObjectRegistry::FCSLock423::FCSLock423()
{
#if ENGINE_MINOR_VERSION >= 23
	UObjectRegistry::Critical.Lock();
#endif
}

UObjectRegistry::FCSLock423::~FCSLock423()
{
#if ENGINE_MINOR_VERSION >= 23
	UObjectRegistry::Critical.Unlock();
#endif
}

namespace ObjectRegistry
{
template<typename F>
void EachClass(UObject* Object, UClass* StopClass, const F& f)
{
	check(StopClass);
	auto ObjectClass = Object->GetClass();
	for (auto CurClass = ObjectClass; CurClass; CurClass = CurClass->GetSuperClass())
	{
#if WITH_EDITOR
		if (CurClass->GetName().StartsWith(TEXT("SKEL_")))
			continue;
#endif
		if (CurClass == StopClass)
			break;
		f(CurClass);
	}
}
template<typename F>
void EachClass(UObject* Object, const F& f)
{
	auto ObjectClass = Object->GetClass();
	for (auto CurClass = ObjectClass; CurClass; CurClass = CurClass->GetSuperClass())
	{
#if WITH_EDITOR
		if (CurClass->GetName().StartsWith(TEXT("SKEL_")))
			continue;
#endif
		f(CurClass);
		if (CurClass->IsNative())
			break;
	}
}

TArray<FObjectRegistryType> Storage = {{}};
auto AllocNewStorage(int32& Index)
{
	if (Index == 0)
	{
		Index = Storage.Add(FObjectRegistryType{MakeShared<FWeakObjectArray>()});
	}
	return Index;
}
TMap<TWeakObjectPtr<UClass>, int32> ClassToID;
}  // namespace ObjectRegistry

UClass* UObjectRegistry::FindFirstNativeClass(UClass* Class)
{
	for (; Class; Class = Class->GetSuperClass())
	{
		if (0 != (Class->ClassFlags & CLASS_Native))
		{
			break;
		}
	}
	return Class;
}

#if WITH_EDITOR
bool UObjectRegistry::EditorIsGameWorld(const UObject* WorldContextObj)
{
	if (IsRunningCommandlet() || !UObjectInitialized() || IsEngineExitRequested())
		return false;

	check(WorldContextObj);
	// 	auto Class = WorldContextObj->GetClass();
	// 	if (IsValid(Class))
	// 	{
	// 		if (Class->IsChildOf(UActorComponent::StaticClass()))
	// 		{
	// 			auto Comp = static_cast<const UActorComponent*>(WorldContextObj);
	// 			if (!IsValid(Comp->GetOuter()) || Comp->GetOuter()->GetFName().IsNone())
	// 				return false;
	// 			if (!IsValid(Comp->GetOwner()) || Comp->GetOwner()->GetFName().IsNone())
	// 				return false;
	// 		}
	// 	}

	auto World = GEngine->GetWorldFromContextObject(WorldContextObj, EGetWorldErrorMode::ReturnNull);
	return IsValid(World) && World->IsGameWorld();
}
#endif

FWeakObjectArray::TIterator FObjectRegistryType::CreateIterator()
{
	check(IsValid());
	return Objects->CreateIterator();
}

void UObjectRegistry::SetObject(UObject* Object, UClass* StopClass)
{
	if (!EditorIsGameWorld(Object))
		return;

	auto& RefObjects = Get(Object)->TypeObjects;
	if (StopClass)
	{
		check(Object->IsA(StopClass));
		ObjectRegistry::EachClass(Object, StopClass->GetSuperClass(), [&](auto CurClass) {
			auto& Ref = RefObjects.FindOrAdd(CurClass);
			ensureMsgf(!Ref.IsValid(), TEXT("%s"), *GetNameSafe(Object));
			Ref = Object;
		});
	}
	else
	{
		ObjectRegistry::EachClass(Object, [&](auto CurClass) {
			auto& Ref = RefObjects.FindOrAdd(CurClass);
			ensureMsgf(!Ref.IsValid(), TEXT("%s"), *GetNameSafe(Object));
			Ref = Object;
		});
	}
}

FWeakObjectArray::TIterator UObjectRegistry::NativeIterator(int32 Index)
{
	if (ensure(Index > 0 && Index < ObjectRegistry::Storage.Num()))
	{
		return ObjectRegistry::Storage[Index].CreateIterator();
	}
	else
	{
		static FWeakObjectArray NullObjects;
		return NullObjects.CreateIterator();
	}
}

UObjectRegistry::UObjectRegistry()
{
	static bool bListened = false;
	if (!bListened)
	{
		bListened = true;
		FWorldDelegates::OnWorldCleanup.AddLambda([](UWorld* World, bool /*bSessionEnded*/, bool /*bCleanupResources*/) {
			for (auto It = ObjectRegistry::ClassToID.CreateIterator(); It;)
			{
				if (!It->Key.IsValid())
					It.RemoveCurrent();
				else
					++It;
			}
#if !WITH_EDITOR
			FCSLock423 Lock;

			if (auto Mgr = Get(World))
				Mgr->Binddings.Empty();
#endif
		});
	}
}

UObjectRegistry* UObjectRegistry::Get(const UObject* Obj)
{
	if (!UObjectInitialized() || IsEngineExitRequested())
		return nullptr;

#if WITH_EDITOR
	if (GIsEditor)
	{
		return UGenericSingletons::GetSingleton<UObjectRegistry>(Obj);
	}
	else
#endif
	{
		return UGenericSingletons::GetSingleton<UObjectRegistry>(nullptr);
	}
}

TArray<UObject*> UObjectRegistry::AllObject(const UObject* WorldContextObj, UClass* Class)
{
	TArray<UObject*> Ret;
	EachObject(WorldContextObj, Class, [&](UObject* Obj) { Ret.Add(Obj); });
	return Ret;
}

void UObjectRegistry::EachObject(const UObject* WorldContextObj, UClass* Class, const TFunctionRef<void(UObject*)>& f)
{
	if (!EditorIsGameWorld(WorldContextObj))
		return;

	if (auto Found = Get(WorldContextObj)->Binddings.Find(Class))
	{
		for (auto It = Found->CreateIterator(); It;)
		{
			if (auto a = It->Get())
			{
				f(a);
				++It;
			}
			else
			{
				It.RemoveCurrent();
			}
		}
	}
}

int32 UObjectRegistry::GetTypeID(UClass* Class)
{
	if (ensure(Class && ObjectRegistry::ClassToID.Contains(Class)))
	{
		return ObjectRegistry::ClassToID[Class];
	}
	return 0;
}

UObject* UObjectRegistry::GetObject(const UObject* WorldContextObj, UClass* Class)
{
	if (!EditorIsGameWorld(WorldContextObj))
		return nullptr;

	if (auto Find = Get(WorldContextObj)->TypeObjects.Find(Class))
		return Find->Get();
	return nullptr;
}

bool UObjectRegistry::AddClassToRegistry(UObject* Object, UClass* StopClass, int32& ID)
{
	if (Object->GetClass() == StopClass)
	{
		ID = ObjectRegistry::AllocNewStorage(ObjectRegistry::ClassToID.FindOrAdd(StopClass));
		return true;
	}
	{
		FCSLock423 Lock;
		ObjectRegistry::EachClass(Object, StopClass, [&](auto CurClass) { ObjectRegistry::AllocNewStorage(ObjectRegistry::ClassToID.FindOrAdd(CurClass)); });
		return false;
	}
}

bool UObjectRegistry::RemoveClassFromRegistry(UObject* Object, UClass* StopClass)
{
	if (!EditorIsGameWorld(Object))
		return true;

	FCSLock423 Lock;
	auto Class = Object->GetClass();
#if WITH_EDITOR
	if (!IsValid(Class) || Class->GetFName().IsNone() || Class->GetName().StartsWith(TEXT("SKEL_")))
		return true;
#endif
	auto Count = ObjectRegistry::ClassToID.Remove(Class);
	return ensure(Count > 0);
}

void UObjectRegistry::AddObjectToRegistry(UObject* Object, UClass* StopClass)
{
	// 	if (!EditorIsGameWorld(Object))
	// 		return;

	FCSLock423 Lock;
	check(!Object->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject));

	ObjectRegistry::EachClass(Object, StopClass->GetSuperClass(), [&](auto CurClass) {
#if WITH_EDITOR
		check(ObjectRegistry::ClassToID.Contains(CurClass));
		check(ObjectRegistry::Storage.IsValidIndex(ObjectRegistry::ClassToID[CurClass]));
#endif
		FObjectRegistryType& Found = Binddings.FindOrAdd(CurClass);
		if (!Found.Objects)
		{
			Found.Objects = ObjectRegistry::Storage[ObjectRegistry::ClassToID.FindChecked(CurClass)].Objects;
		}
#if WITH_EDITOR
		ensure(Found.Objects.IsValid() && !Found.Objects->Contains(Object));
#endif
		Found.Add(Object);
	});
}

void UObjectRegistry::RemoveObjectFromRegistry(UObject* Object, UClass* StopClass)
{
	// 	if (!EditorIsGameWorld(Object))
	// 		return;

	FCSLock423 Lock;

	check(!Object->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject));

	auto ObjectClass = Object->GetClass();
	ObjectRegistry::EachClass(Object, StopClass->GetSuperClass(), [&](auto CurClass) {
		if (auto Found = Binddings.Find(CurClass))
		{
			Found->Remove(Object);
		}
	});
}
