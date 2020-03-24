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

#include "GenericSingletons.h"

#include "ClassDataStorage.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/GameEngine.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Launch/Resources/Version.h"
#include "UObject/UObjectGlobals.h"
#include "WorldLocalStorages.h"

//////////////////////////////////////////////////////////////////////////
namespace GenericSingletons
{
UObject* DynamicReflectionImpl(const FString& TypeName, UClass* TypeClass)
{
	TypeClass = TypeClass ? TypeClass : UObject::StaticClass();
	bool bIsValidName = true;
	FString FailureReason;
	if (TypeName.Contains(TEXT(" ")))
	{
		FailureReason = FString::Printf(TEXT("contains a space."));
		bIsValidName = false;
	}
	else if (!FPackageName::IsShortPackageName(TypeName))
	{
		if (TypeName.Contains(TEXT(".")))
		{
			FString PackageName;
			FString ObjectName;
			TypeName.Split(TEXT("."), &PackageName, &ObjectName);

			const bool bIncludeReadOnlyRoots = true;
			FText Reason;
			if (!FPackageName::IsValidLongPackageName(PackageName, bIncludeReadOnlyRoots, &Reason))
			{
				FailureReason = Reason.ToString();
				bIsValidName = false;
			}
		}
		else
		{
			FailureReason = TEXT("names with a path must contain a dot. (i.e /Script/Engine.StaticMeshActor)");
			bIsValidName = false;
		}
	}

	UObject* NewReflection = nullptr;
	if (bIsValidName)
	{
		UObject* ClassPackage = ANY_PACKAGE;
		if (FPackageName::IsShortPackageName(TypeName))
		{
			NewReflection = StaticFindObject(TypeClass, ClassPackage, *TypeName);
		}
		else
		{
			NewReflection = StaticFindObject(TypeClass, nullptr, *TypeName);
		}

		if (!NewReflection)
		{
			if (UObjectRedirector* RenamedClassRedirector = FindObject<UObjectRedirector>(ClassPackage, *TypeName))
			{
				NewReflection = RenamedClassRedirector->DestinationObject;
			}
		}

		if (!NewReflection)
			FailureReason = TEXT("Failed to find class.");
	}

	return NewReflection;
}

void SetWorldCleanup(FSimpleDelegate Cb, bool EditorOnly)
{
	if ((!EditorOnly || !GIsEditor))
	{
#if WITH_EDITOR
		if (GIsEditor)
		{
			FEditorDelegates::PreBeginPIE.AddLambda([Cb{MoveTemp(Cb)}](const bool) { Cb.ExecuteIfBound(); });
		}
		else
#endif
		{
			// cleanup
			// Register for PreloadMap, so cleanup can occur on map transitions
			FCoreUObjectDelegates::PreLoadMap.AddLambda([Cb{MoveTemp(Cb)}](const FString&) { Cb.ExecuteIfBound(); });
			// FWorldDelegates::OnPreWorldFinishDestroy.AddLambda([](UWorld*) { Storage.Cleanup(); });
		}
	}
}
}  // namespace GenericSingletons

UGenericSingletons* UGenericSingletons::GetManager(UWorld* World)
{
	return WorldLocalStorages::GetObject<UGenericSingletons>(World);
}

UGenericSingletons::UGenericSingletons()
{
	if (TrueOnFirstCall([] {}))
	{
		// GEngine->OnWorldAdded();
		// GEngine->OnWorldDestroyed();
		FWorldDelegates::OnWorldCleanup.AddLambda([](UWorld* World, bool /*bSessionEnded*/, bool /*bCleanupResources*/) { WorldLocalStorages::RemoveObject<UGenericSingletons>(World); });
		// 	FWorldDelegates::OnPreWorldInitialization.AddLambda([](UWorld* Wrold, const UWorld::InitializationValues IVS) { GetManager(Wrold); });
	}
}

#if !UE_SERVER
#	include "Blueprint/UserWidget.h"
#endif

#if WITH_EDITOR
#	include "Editor.h"
#endif

#include "Misc/PackageName.h"

UObject* UGenericSingletons::RegisterAsSingletonImpl(UObject* Object, const UObject* WorldContextObject, bool bReplaceExist, UClass* InNativeClass)
{
	check(IsValid(Object));
	if (!ensureAlwaysMsgf(!InNativeClass || InNativeClass->IsNative() || !Object->IsA(InNativeClass), TEXT("Object %s is not child class of %s"), *GetNameSafe(Object), *GetNameSafe(InNativeClass)))
	{
		return nullptr;
	}

	auto World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	// skip cook and CDOs without world
	if (IsRunningCommandlet() || (!World && Object->HasAnyFlags(RF_ClassDefaultObject)))
	{
		UE_LOG(LogTemp, Warning, TEXT("UGenericSingletons::RegisterAsSingleton Error"));
		return nullptr;
	}
	auto Mgr = GetManager(World);
	auto ObjectClass = Object->GetClass();

	UObject* LastPtr = nullptr;
	if (!bReplaceExist)
	{
		LastPtr = Mgr->Singletons.FindOrAdd(ObjectClass);
		if (!IsValid(LastPtr) && InNativeClass)
			LastPtr = Mgr->Singletons.FindOrAdd(InNativeClass);
		if (IsValid(LastPtr))
		{
			UE_LOG(LogTemp, Log, TEXT("UGenericSingletons::RegisterAsSingleton Exist %s(%p) -> %s -> %s(%p)"), *GetTypedNameSafe(World), World, *ObjectClass->GetName(), *GetTypedNameSafe(LastPtr), LastPtr);
			return LastPtr;
		}
	}

	for (auto CurClass = ObjectClass; CurClass; CurClass = CurClass->GetSuperClass())
	{
		UObject*& Ptr = Mgr->Singletons.FindOrAdd(CurClass);
		LastPtr = IsValid(Ptr) ? Ptr : nullptr;

		ensureAlways(!bReplaceExist || !LastPtr || (!InNativeClass && CurClass->IsNative()) || InNativeClass /* == CurClass*/);

		{
			Ptr = Object;
#if !UE_BUILD_SHIPPING
			UE_LOG(LogTemp, Log, TEXT("UGenericSingletons::RegisterAsSingleton %s(%p) -> %s -> %s(%p)"), *GetTypedNameSafe(World), World, *CurClass->GetName(), *GetTypedNameSafe(Ptr), Ptr);
#endif
		}

		if ((InNativeClass && CurClass == InNativeClass) || (!InNativeClass && CurClass->IsNative()))
			break;
	}

	return LastPtr;
}

UObject* UGenericSingletons::GetSingletonImpl(UClass* Class, const UObject* WorldContextObject, bool bCreate, UClass* RegClass)
{
	check(Class);
	if (!RegClass)
		RegClass = Class;

	auto World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	auto Mgr = GetManager(World);
	UObject*& Ptr = Mgr->Singletons.FindOrAdd(RegClass);
#if 0
	UE_LOG(LogTemp, Log, TEXT("UGenericSingletons::GetSingleton %s(%p) -> %s -> %s(%p)"),
		*GetTypedNameSafe(World), World, *Class->GetName(), *GetTypedNameSafe(Ptr), Ptr);

#endif
	if (!IsValid(Ptr) && bCreate)
	{
		Ptr = CreateInstanceImpl(World, Class);
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Log, TEXT("UGenericSingletons::NewSingleton %s(%p) -> %s -> %s(%p)"), *GetTypedNameSafe(World), World, *Class->GetName(), *GetTypedNameSafe(Ptr), Ptr);

#endif
		if (ensureAlways(IsValid(Ptr)))
		{
			RegisterAsSingletonImpl(Ptr, World, true, RegClass);
		}
		else
		{
			Ptr = nullptr;
		}
	}

	return Ptr;
}

UObject* UGenericSingletons::CreateInstanceImpl(const UObject* WorldContextObject, UClass* Class)
{
	check(Class);
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	UObject* Ptr = nullptr;
	bool bIsActorClass = Class->IsChildOf(AActor::StaticClass());

#if WITH_EDITOR
	FName InstName = MakeUniqueObjectName(nullptr, Class, TEXT("GSInst_"));
#else
	FName InstName = NAME_None;
#endif

	if (!IsValid(World))
	{
		ensureAlwaysMsgf(!bIsActorClass, TEXT("world not existed!!!"));
		auto Instance = WorldLocalStorages::FindGameInstance();
		if (ensure(Instance))
		{
			Ptr = NewObject<UObject>(Instance, Class, InstName);
		}
		else
		{
			Ptr = NewObject<UObject>(GetTransientPackage(), Class, InstName);
		}
	}
	else if (!bIsActorClass)
	{
#if !UE_SERVER
		if (Class->IsChildOf(UUserWidget::StaticClass()))
		{
			Ptr = CreateWidget(World, Class, InstName);
		}
		else
#endif
		{
			Ptr = NewObject<UObject>(World, Class, InstName);
		}
	}
	else
	{
		FActorSpawnParameters ActorSpawnParameters;
		ActorSpawnParameters.Name = InstName;
		Ptr = World->SpawnActor<AActor>(Class, ActorSpawnParameters);
	}
	ensureAlways(IsValid(Ptr));

#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Log, TEXT("UGenericSingletons::CreateInstanceImpl %s(%p) -> %s -> %s(%p)"), *GetTypedNameSafe(World), World, *Class->GetName(), *GetTypedNameSafe(Ptr), Ptr);
#endif
	return Ptr;
}

TSharedPtr<struct FStreamableHandle> UGenericSingletons::AsyncLoad(const FString& InPath, FAsyncObjectCallback DelegateCallback, bool bSkipInvalid, TAsyncLoadPriority Priority)
{
	auto& StreamableMgr = UAssetManager::GetStreamableManager();
	FSoftObjectPath SoftPath = InPath;
	return UAssetManager::GetStreamableManager().RequestAsyncLoad(SoftPath,
																  FStreamableDelegate::CreateLambda([bSkipInvalid, Cb{MoveTemp(DelegateCallback)}, SoftPath] {
																	  auto* Obj = SoftPath.ResolveObject();
																	  if (!bSkipInvalid || Obj)
																		  Cb.ExecuteIfBound(Obj);
																  }),
																  Priority,
																  true
#if UE_BUILD_DEBUG
																  ,
																  false,
																  FString::Printf(TEXT("RequestAsyncLoad [%s]"), *InPath)
#endif
	);
}

TSharedPtr<struct FStreamableHandle> UGenericSingletons::AsyncLoad(const TArray<FSoftObjectPath>& InPaths, FAsyncBatchCallback DelegateCallback, bool bSkipInvalid, TAsyncLoadPriority Priority)
{
	auto& StreamableMgr = UAssetManager::GetStreamableManager();
	return UAssetManager::GetStreamableManager().RequestAsyncLoad(InPaths,
																  FStreamableDelegate::CreateLambda([bSkipInvalid, Cb{MoveTemp(DelegateCallback)}, Paths{InPaths}] {
																	  TArray<UObject*> Loaded;
																	  Loaded.Reserve(Paths.Num());
																	  for (int32 i = 0; i < Paths.Num(); ++i)
																	  {
																		  auto Obj = Paths[i].ResolveObject();
																		  if (!bSkipInvalid || IsValid(Obj))
																			  Loaded.Add(Obj);
																	  }
																	  Cb.ExecuteIfBound(Loaded);
																  }),
																  Priority,
																  true);
}

bool UGenericSingletons::AsyncCreate(const UObject* BindedObject, const FString& InPath, FAsyncObjectCallback Cb)
{
	FWeakObjectPtr WeakObj = BindedObject;
	auto Lambda = [WeakObj, Cb{MoveTemp(Cb)}](UObject* ResolvedObj) {
		if (!WeakObj.IsStale())
		{
			auto ResolvedClass = Cast<UClass>(ResolvedObj);
			UObject* Obj = CreateInstanceImpl(WeakObj.Get(), ResolvedClass);
			Cb.ExecuteIfBound(Obj);
		}
	};

	auto Handle = UGenericSingletons::AsyncLoad(InPath, FAsyncObjectCallback::CreateLambda(Lambda));

	return Handle.IsValid();
}
