// Copyright 2018-2020 wangjieest, Inc. All Rights Reserved.

#include "ObjectPattern.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GenericSingletons.h"
#include "UnrealCompatibility.h"

FCriticalSection UObjectPattern::Critical;

UObjectPattern::FCSLock423::FCSLock423()
{
#if ENGINE_MINOR_VERSION >= 23
	UObjectPattern::Critical.Lock();
#endif
}

UObjectPattern::FCSLock423::~FCSLock423()
{
#if ENGINE_MINOR_VERSION >= 23
	UObjectPattern::Critical.Unlock();
#endif
}

namespace ObjectPattern
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

TArray<FObjectPatternType> Storage = {{}};
auto AllocNewStorage(int32& Index)
{
	if (Index == 0)
	{
		Index = Storage.Add(FObjectPatternType{MakeShared<FWeakObjectArray>()});
	}
	return Index;
}
TMap<TWeakObjectPtr<UClass>, int32> ClassToID;
}  // namespace ObjectPattern

UClass* UObjectPattern::FindFirstNativeClass(UClass* Class)
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
bool UObjectPattern::EditorIsGameWorld(const UObject* WorldContextObj)
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

FWeakObjectArray::TIterator FObjectPatternType::CreateIterator()
{
	check(IsValid());
	return Objects->CreateIterator();
}

void UObjectPattern::SetObject(UObject* Object, UClass* StopClass)
{
	if (!EditorIsGameWorld(Object))
		return;

	auto& RefObjects = Get(Object)->TypeObjects;
	if (StopClass)
	{
		check(Object->IsA(StopClass));
		ObjectPattern::EachClass(Object, StopClass->GetSuperClass(), [&](auto CurClass) {
			auto& Ref = RefObjects.FindOrAdd(CurClass);
			ensureMsgf(!Ref.IsValid(), TEXT("%s"), *GetNameSafe(Object));
			Ref = Object;
		});
	}
	else
	{
		ObjectPattern::EachClass(Object, [&](auto CurClass) {
			auto& Ref = RefObjects.FindOrAdd(CurClass);
			ensureMsgf(!Ref.IsValid(), TEXT("%s"), *GetNameSafe(Object));
			Ref = Object;
		});
	}
}

FWeakObjectArray::TIterator UObjectPattern::NativeIterator(int32 Index)
{
	if (ensure(Index > 0 && Index < ObjectPattern::Storage.Num()))
	{
		return ObjectPattern::Storage[Index].CreateIterator();
	}
	else
	{
		static FWeakObjectArray NullObjects;
		return NullObjects.CreateIterator();
	}
}

UObjectPattern::UObjectPattern()
{
	static bool bListened = false;
	if (!bListened)
	{
		bListened = true;
		FWorldDelegates::OnWorldCleanup.AddLambda([](UWorld* World, bool /*bSessionEnded*/, bool /*bCleanupResources*/) {
			for (auto It = ObjectPattern::ClassToID.CreateIterator(); It;)
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

UObjectPattern* UObjectPattern::Get(const UObject* Obj)
{
	if (!UObjectInitialized() || IsEngineExitRequested())
		return nullptr;

#if WITH_EDITOR
	if (GIsEditor)
	{
		return UGenericSingletons::GetSingleton<UObjectPattern>(Obj);
	}
	else
#endif
	{
		return UGenericSingletons::GetSingleton<UObjectPattern>(nullptr);
	}
}

TArray<UObject*> UObjectPattern::AllObject(const UObject* WorldContextObj, UClass* Class)
{
	TArray<UObject*> Ret;
	EachObject(WorldContextObj, Class, [&](UObject* Obj) { Ret.Add(Obj); });
	return Ret;
}

void UObjectPattern::EachObject(const UObject* WorldContextObj, UClass* Class, const TFunctionRef<void(UObject*)>& f)
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

int32 UObjectPattern::GetTypeID(UClass* Class)
{
	if (ensure(Class && ObjectPattern::ClassToID.Contains(Class)))
	{
		return ObjectPattern::ClassToID[Class];
	}
	return 0;
}

UObject* UObjectPattern::GetObject(const UObject* WorldContextObj, UClass* Class)
{
	if (!EditorIsGameWorld(WorldContextObj))
		return nullptr;

	if (auto Find = Get(WorldContextObj)->TypeObjects.Find(Class))
		return Find->Get();
	return nullptr;
}

bool UObjectPattern::AddClassToRegistry(UObject* Object, UClass* StopClass, int32& ID)
{
	if (Object->GetClass() == StopClass)
	{
		ID = ObjectPattern::AllocNewStorage(ObjectPattern::ClassToID.FindOrAdd(StopClass));
		return true;
	}
	{
		FCSLock423 Lock;
		ObjectPattern::EachClass(Object, StopClass, [&](auto CurClass) { ObjectPattern::AllocNewStorage(ObjectPattern::ClassToID.FindOrAdd(CurClass)); });
		return false;
	}
}

bool UObjectPattern::RemoveClassFromRegistry(UObject* Object, UClass* StopClass)
{
	if (!EditorIsGameWorld(Object))
		return true;

	FCSLock423 Lock;
	auto Class = Object->GetClass();
#if WITH_EDITOR
	if (!IsValid(Class) || Class->GetFName().IsNone() || Class->GetName().StartsWith(TEXT("SKEL_")))
		return true;
#endif
	auto Count = ObjectPattern::ClassToID.Remove(Class);
	return ensure(Count > 0);
}

void UObjectPattern::AddObjectToRegistry(UObject* Object, UClass* StopClass)
{
	// 	if (!EditorIsGameWorld(Object))
	// 		return;

	FCSLock423 Lock;
	check(!Object->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject));

	ObjectPattern::EachClass(Object, StopClass->GetSuperClass(), [&](auto CurClass) {
#if WITH_EDITOR
		check(ObjectPattern::ClassToID.Contains(CurClass));
		check(ObjectPattern::Storage.IsValidIndex(ObjectPattern::ClassToID[CurClass]));
#endif
		FObjectPatternType& Found = Binddings.FindOrAdd(CurClass);
		if (!Found.Objects)
		{
			Found.Objects = ObjectPattern::Storage[ObjectPattern::ClassToID.FindChecked(CurClass)].Objects;
		}
#if WITH_EDITOR
		ensure(Found.Objects.IsValid() && !Found.Objects->Contains(Object));
#endif
		Found.Add(Object);
	});
}

void UObjectPattern::RemoveObjectFromRegistry(UObject* Object, UClass* StopClass)
{
	// 	if (!EditorIsGameWorld(Object))
	// 		return;

	FCSLock423 Lock;

	check(!Object->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject));

	auto ObjectClass = Object->GetClass();
	ObjectPattern::EachClass(Object, StopClass->GetSuperClass(), [&](auto CurClass) {
		if (auto Found = Binddings.Find(CurClass))
		{
			Found->Remove(Object);
		}
	});
}
