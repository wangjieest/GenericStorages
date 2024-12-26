// Copyright GenericStorages, Inc. All Rights Reserved.

#include "GenericSingletons.h"

#include "ClassDataStorage.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/GameEngine.h"
#include "Engine/Level.h"
#include "Engine/ObjectReferencer.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GenericStoragesLog.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "Runtime/Launch/Resources/Version.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectThreadContext.h"
#include "WorldLocalStorages.h"

#if !UE_SERVER
#include "Blueprint/UserWidget.h"
#endif

#if WITH_EDITOR
#include "Editor.h"
#include "GameDelegates.h"
#endif

namespace GenericStorages
{
GENERICSTORAGES_API UWorld* GetGameWorldChecked(bool bEnsureGameWorld)
{
	auto World = GWorld;
#if WITH_EDITOR
	if (GIsEditor && !GIsPlayInEditorWorld)
	{
		static auto FindFirstPIEWorld = [] {
			UWorld* World = nullptr;
			auto& WorldContexts = GEngine->GetWorldContexts();
			for (const FWorldContext& Context : WorldContexts)
			{
				auto CurWorld = Context.World();
				if (IsValid(CurWorld) && (CurWorld->WorldType == EWorldType::PIE /* || CurWorld->WorldType == EWorldType::Game*/))
				{
					World = CurWorld;
					break;
				}
			}

			// ensure(World);
			return World;
		};

		FWorldContext* WorldContext = GEngine->GetWorldContextFromPIEInstance(FMath::Max(0, (int32)GPlayInEditorID));
		if (WorldContext && ensure(WorldContext->WorldType == EWorldType::PIE /* || WorldContext->WorldType == EWorldType::Game*/))
		{
			World = WorldContext->World();
		}
		else
		{
			World = FindFirstPIEWorld();
		}

		if (!World && !bEnsureGameWorld)
		{
			World = GEditor->GetEditorWorldContext().World();
		}
	}
#else
	check(World);
#endif
	return World;
}
UGameInstance* FindGameInstance()
{
	UGameInstance* Instance = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		ensureAlwaysMsgf(!GIsInitialLoad && GEngine, TEXT("Is it needed to get singleton before engine initialized?"));
		UWorld* World = GenericStorages::GetGameWorldChecked(false);
		Instance = World ? World->GetGameInstance() : nullptr;
	}
	else
#endif
	{
		if (UGameEngine* GameEngine = Cast<UGameEngine>(GEngine))
		{
			Instance = GameEngine->GameInstance;
		}
	}
	return Instance;
}
UObject* CreateInstanceImpl(const UObject* WorldContextObject, const FObjConstructParameter& Parameter, UObject** OutCtx = nullptr)
{
	auto Class = Parameter.Class;
	check(Class);
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	UObject* Ctx = World;
	bool bIsActorClass = Class->IsChildOf(AActor::StaticClass());
	UObject* Ptr = nullptr;

	FName InstName = Parameter.Name;
	const bool bNameNone = Parameter.Name.IsNone();

	do
	{
#if WITH_EDITOR
		static uint64 GSIns_Index = 0;
#endif
		if (!IsValid(World))
		{
			ensureAlwaysMsgf(!bIsActorClass, TEXT("cannot create actor while world not existed!!!"));
			UGameInstance* Instance = GenericStorages::FindGameInstance();
#ifdef PLATFORM_LINUX
#else
			ensure(Instance);
#endif
#if WITH_EDITOR
			if (bNameNone)
				InstName = FName(*FString::Printf(TEXT("GSIns_%s"), *GetNameSafe(Class)), ++GSIns_Index);
#endif
			Ctx = Instance ? (UObject*)Instance : (UObject*)GetTransientPackage();
			Ptr = NewObject<UObject>(Ctx, Class, InstName);
		}
		else
		{
#if WITH_EDITOR
			if (bNameNone)
				InstName = FName(*FString::Printf(TEXT("GSWorld_%s"), *GetNameSafe(Class)), ++GSIns_Index);
#endif
			UGameInstance* Instance = World->GetGameInstance();
			ULevel* Level = (ULevel*)Cast<ULevel>(WorldContextObject);
			Level = Level ? Level : ToRawPtr(World->PersistentLevel);
			if (!bNameNone)
			{
				Ptr = StaticFindObjectFast(Class, Level, Parameter.Name);
				if (Ptr && !IsValid(Ptr))
				{
					Ptr->Rename(nullptr, Level, REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional | REN_ForceNoResetLoaders);
					Ptr = nullptr;
				}
				if (Ptr)
					break;
			}

			if (!bIsActorClass)
			{
				Ctx = Instance ? (UObject*)Instance : (UObject*)World;
#if !UE_SERVER
				if (Class->IsChildOf(UUserWidget::StaticClass()))
				{
					if (Instance)
					{
#if !UE_4_20_OR_LATER
						Ptr = CreateWidget<UUserWidget>(Instance, Class);
#else
						Ptr = CreateWidget(Instance, Class, InstName);
#endif
					}
					else
					{
#if !UE_4_20_OR_LATER
						Ptr = CreateWidget<UUserWidget>(World, Class);
#else
						Ptr = CreateWidget(World, Class, InstName);
#endif
					}
				}
				else
#endif
				{
					Ptr = NewObject<UObject>(Ctx, Class, InstName);
				}
			}
			else
			{
				FActorSpawnParameters ActorSpawnParameters;
				ActorSpawnParameters.Name = InstName;
				ActorSpawnParameters.OverrideLevel = Level;
				Ptr = World->SpawnActor(Class, Parameter.Trans, ActorSpawnParameters);
			}
		}

	} while (false);

	if (OutCtx)
		*OutCtx = Ctx;

	ensureAlways(Ptr && Ptr->IsValidLowLevelFast());

	if (Ptr && Ptr->Implements<UGenericInstanceInc>())
	{
		IGenericInstanceInc::Execute_OnGencreicInstanceCreated(Ptr, Ctx);
	}

	return Ptr;
}
UGenericSingletons* GetSingletonsManager(const UObject* InObj)
{
	if (!ensure(!IsGarbageCollecting()))
		return nullptr;

#if WITH_EDITOR || 1
	return WorldLocalStorages::GetLocalValue<UGenericSingletons>(InObj);
#else
	static TWeakObjectPtr<UGenericSingletons> WeakSingleton;
	if (auto CachedObj = WeakSingleton.Get())
		return CachedObj;
	auto Obj = WorldLocalStorages::GetLocalValue<UGenericSingletons>(InObj);
	WeakSingleton = Obj;
	return Obj;
#endif
}

#if WITH_EDITOR
void CallOnEditorMapOpendImpl(TDelegate<void(UWorld*)> Delegate)
{
	FEditorDelegates::OnMapOpened.AddLambda([Cb(MoveTemp(Delegate))](auto&&...) {
		if (GIsEditor && ensure(GWorld))
		{
			Cb.Execute(GWorld);
		}
	});
}
#endif

FTimerManager* GetTimerManager(UWorld* InWorld)
{
#if WITH_EDITOR
	if (GIsEditor && (!InWorld || !InWorld->IsGameWorld()))
	{
		return &GEditor->GetTimerManager().Get();
	}
	else
#endif
	{
		check(InWorld || GWorld);
		return IsValid(InWorld) ? &InWorld->GetTimerManager() : &GWorld->GetTimerManager();
	}
}

FTimerHandle SetTimer(UWorld* InWorld, FTimerDelegate const& InDelegate, float InRate, bool InbLoop, float InFirstDelay /*= -1.f*/)
{
	FTimerHandle TimerHandle;
	auto TimerMgr = GetTimerManager(InWorld);
	if (ensure(TimerMgr))
	{
		TimerMgr->SetTimer(TimerHandle, InDelegate, InRate, InbLoop, InFirstDelay);
	}
	return TimerHandle;
}

UObject* CreateSingletonImpl(const UObject* WorldContextObject, UClass* Class, UObject*& OutCtx)
{
	FName InstName = FName(*FString::Printf(TEXT("GSOne_%s"), *GetNameSafe(Class)));
	return GenericStorages::CreateInstanceImpl(WorldContextObject, {Class, InstName}, &OutCtx);
}

}  // namespace GenericStorages

namespace WorldLocalStorages
{
UObject* CreateInstanceImpl(const UObject* WorldContextObject, UClass* InClass)
{
	return GenericStorages::CreateInstanceImpl(WorldContextObject, {InClass});
}
}  // namespace WorldLocalStorages

//////////////////////////////////////////////////////////////////////////
#if WITH_EDITOR
UWorld* UGenericLocalStoreEditor::GetEditorWorld(bool bEnsureIsGWorld)
{
	if (GEditor)
	{
		for (auto& Ctx : GEditor->GetWorldContexts())
		{
			if (Ctx.WorldType == EWorldType::Editor)
			{
				ensure(!bEnsureIsGWorld || Ctx.World() == GWorld);
				return Ctx.World();
			}
		}
	}
	return nullptr;
}
#endif

UWorld* UGenericLocalStore::GetGameWorldChecked()
{
	return GenericStorages::GetGameWorldChecked(true);
}

void UGenericLocalStore::BindObjectReference(UObject* InCtx, UObject* Obj)
{
	check(IsValid(Obj));
	if (!IsValid(InCtx))
	{
		auto Instance = GenericStorages::FindGameInstance();
		ensureAlwaysMsgf(GIsEditor || Instance != nullptr, TEXT("GameInstance Error"));
#if UE_4_20_OR_LATER
		if (Instance)
		{
			Instance->RegisterReferencedObject(Obj);
		}
		else
#endif
		{
			UE_LOG(LogGenericStorages, Log, TEXT("UGenericLocalStore::AddObjectReference RootObject Added [%s]"), *GetNameSafe(Obj));
			Obj->AddToRoot();
#if WITH_EDITOR
			// FGameDelegates::Get().GetEndPlayMapDelegate().Add(CreateWeakLambda(Obj, [Obj] { Obj->RemoveFromRoot(); }));
			FEditorDelegates::EndPIE.Add(CreateWeakLambda(Obj, [Obj](const bool) {
				UE_LOG(LogGenericStorages, Log, TEXT("UGenericLocalStore::AddObjectReference RootObject Removed [%s]"), *GetNameSafe(Obj));
				Obj->RemoveFromRoot();
			}));
#endif
		}
	}
	else if (UGameInstance* Instance = Cast<UGameInstance>(InCtx))
	{
		Instance->RegisterReferencedObject(Obj);
	}
#if WITH_EDITOR
	else if (Cast<UWorld>(InCtx) && Cast<UWorld>(InCtx)->IsGameWorld())
	{
		Cast<UWorld>(InCtx)->PerModuleDataObjects.AddUnique(Obj);
	}
	else if (UWorld* World = InCtx->GetWorld())
	{
		// EditorWorld
		if (auto Ctx = GEngine->GetWorldContextFromWorld(World))
		{
			static FName ObjName = FName("__GS_Referencers__");
			auto ReferencerPtr = Ctx->ObjectReferencers.FindByPredicate([](class UObjectReferencer* Obj) { return Obj && (Obj->GetFName() == ObjName); });
			if (ReferencerPtr)
			{
				(*ReferencerPtr)->ReferencedObjects.AddUnique(Obj);
			}
			else
			{
				auto Referencer = static_cast<UObjectReferencer*>(NewObject<UObject>(World, DynamicClass(TEXT("ObjectReferencer")), ObjName, RF_Transient));
				Referencer->ReferencedObjects.AddUnique(Obj);
				Ctx->ObjectReferencers.Add(Referencer);
				if (TrueOnFirstCall([] {}))
				{
					FWorldDelegates::OnWorldCleanup.AddLambda([](UWorld* World, bool bSessionEnded, bool bCleanupResources) {
						do
						{
							auto WorldCtx = GEngine->GetWorldContextFromWorld(World);
							if (!WorldCtx)
								break;

							auto Idx = WorldCtx->ObjectReferencers.IndexOfByPredicate([](class UObjectReferencer* Obj) { return Obj && (Obj->GetFName() == ObjName); });
							if (Idx == INDEX_NONE)
								break;
							WorldCtx->ObjectReferencers.RemoveAtSwap(Idx);
						} while (false);
					});
				}
			}
		}
		else
		{
			// Transient World
			World->PerModuleDataObjects.AddUnique(Obj);
		}
	}
#endif
	else if (auto ThisWorld = InCtx->GetWorld())

	{
		ThisWorld->PerModuleDataObjects.AddUnique(Obj);
	}
}

namespace GenericSingletons
{
#if USE_GENEIRC_SINGLETON_GUARD
static TMap<UClass*, int32> SingletonCreatationLevel;
static bool IsInSingletonCreatation(UObject* InObj)
{
	check(InObj);
	auto Find = SingletonCreatationLevel.Find(InObj->GetClass());
	if (Find && *Find > 0)
		return true;

	// ensure not exist
	return !UGenericSingletons::FindSingleton(InObj->GetClass(), InObj);
}
struct FSingletonCreatationScope
{
	FSingletonCreatationScope(UClass* InType)
		: Type(InType)
	{
		++SingletonCreatationLevel.FindOrAdd(Type);
	}
	~FSingletonCreatationScope() { --SingletonCreatationLevel.FindChecked(Type); }

private:
	UClass* Type;
};
#endif

UObject* DynamicReflectionImpl(const FString& TypeName, UClass* TypeClass)
{
	TypeClass = TypeClass ? TypeClass : UObject::StaticClass();
	bool bIsValidName = true;
	FString FailureReason;
	UObject* ClassPackage = nullptr;
	FString ObjectName = TypeName;
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
			TypeName.Split(TEXT("."), &PackageName, &ObjectName);

			const bool bIncludeReadOnlyRoots = true;
			FText Reason;
			if (!FPackageName::IsValidLongPackageName(PackageName, bIncludeReadOnlyRoots, &Reason))
			{
				FailureReason = Reason.ToString();
				bIsValidName = false;
			}
			else
			{
				ClassPackage = LoadPackage(nullptr, *PackageName, LOAD_NoWarn);
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
		if (FPackageName::IsShortPackageName(TypeName))
		{
			ClassPackage = ANY_PACKAGE_COMPATIABLE;
			if (TypeClass->IsChildOf(UEnum::StaticClass()))
				NewReflection = StaticFindObject(TypeClass, ClassPackage, *TypeName.Mid(0, [&] {
					auto Index = TypeName.Find(TEXT("::"));
					return Index == INDEX_NONE ? MAX_int32 : Index;
				}()));
			else
				NewReflection = StaticFindObject(TypeClass, ClassPackage, *TypeName);
		}
		else
		{
			NewReflection = StaticFindObject(TypeClass, ClassPackage, *ObjectName);
		}

		if (!NewReflection)
		{
			if (UObjectRedirector* RenamedClassRedirector = FindObject<UObjectRedirector>(ANY_PACKAGE_COMPATIABLE, *ObjectName))
			{
				NewReflection = RenamedClassRedirector->DestinationObject;
			}
		}

		if (!NewReflection)
			FailureReason = TEXT("failed to find class.");
	}

	return NewReflection;
}

void DeferredWorldCleanup(FSimpleDelegate Cb, FString Desc, bool EditorOnly)
{
	if (!EditorOnly || GIsEditor)
	{
#if WITH_EDITOR
		FGameDelegates::Get().GetEndPlayMapDelegate().Add(Cb);
#endif
		struct FDeferredWorldCleanup
		{
			FSimpleDelegate Cb;
			FString Desc;
			FDeferredWorldCleanup(FSimpleDelegate&& InCb, FString&& InDesc)
				: Cb(MoveTemp(InCb))
				, Desc(MoveTemp(InDesc))
			{
			}

			void operator()(const bool) const { Execute(); }
			void operator()(const FString&) const { Execute(); }
			void Execute() const
			{
				UE_LOG(LogGenericStorages, Log, TEXT("DeferredWorldCleanup %s"), *Desc);
				Cb.ExecuteIfBound();
			}
		} DeferredAction(MoveTemp(Cb), MoveTemp(Desc));

#if WITH_EDITOR
		if (GIsEditor)
		{
			if (auto Obj = Cb.GetUObject())
			{
				FEditorDelegates::PreBeginPIE.Add(CreateWeakLambda<FEditorDelegates::FOnPIEEvent::FDelegate>(Obj, MoveTemp(DeferredAction)));
			}
			else
			{
				FEditorDelegates::PreBeginPIE.AddLambda(MoveTemp(DeferredAction));
			}
		}
#endif
		{
			// Register for PreloadMap, so cleanup can occur on map transitions
			if (auto Obj = Cb.GetUObject())
			{
				FCoreUObjectDelegates::PreLoadMap.Add(CreateWeakLambda<FCoreUObjectDelegates::FPreLoadMapDelegate::FDelegate>(Obj, MoveTemp(DeferredAction)));
			}
			else
			{
				// cleanup
				FCoreUObjectDelegates::PreLoadMap.AddLambda(MoveTemp(DeferredAction));
			}
		}
	}
}
}  // namespace GenericSingletons

#if USE_GENEIRC_SINGLETON_GUARD
GENERICSTORAGES_API bool IsInSingletonCreatation(UObject* InObj)
{
	UE_LOG(LogGenericStorages, Log, TEXT("IsInSingletonCreatation %s"), *GetNameSafe(InObj));
	if (IsRunningCommandlet() || InObj->GetOutermost()->ContainsMap()
#if WITH_EDITOR
		|| (!InObj->GetWorld() && Cast<UWorld>(InObj->GetOutermostObject()))
#endif
	)
	{
		return true;
	}
	return GenericSingletons::IsInSingletonCreatation(InObj);
}
#endif

UGenericSingletons* UGenericSingletons::GetSingletonsManager(const UObject* InObj)
{
	return GenericStorages::GetSingletonsManager(InObj);
}

UGenericSingletons::UGenericSingletons()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		UE_LOG(LogGenericStorages, Log, TEXT("UGenericSingletons Singleton Added To World [%s]"), *GetNameSafe(GetWorld()));
	}
}

UObject* UGenericSingletons::K2_GetSingleton(UClass* Class, const UObject* WorldContextObject, bool bCreate)
{
	return GetSingletonInternal(Class, WorldContextObject, bCreate, nullptr, nullptr);
}

namespace
{
static FDelayedAutoRegisterHelper DelayInnerInitUGMPRpcProxy(EDelayedRegisterRunPhase::EndOfEngineInit, [] {
	// GEngine->OnWorldAdded();
	// GEngine->OnWorldDestroyed();
	// FCoreUObjectDelegates::PreLoadMap.
	FWorldDelegates::OnWorldBeginTearDown.AddLambda([](UWorld* World, auto&&...) {
		UE_LOG(LogGenericStorages, Log, TEXT("UGenericSingletons Singleton Removed World [%s]"), *GetNameSafe(World));
		WorldLocalStorages::RemoveLocalValue<UGenericSingletons>(World);
		World->ExtraReferencedObjects.Remove(nullptr);
		World->PerModuleDataObjects.Remove(nullptr);
	});
#if WITH_EDITOR
#if UE_5_00_OR_LATER
	FEditorDelegates::PreSaveWorldWithContext.AddLambda([](UWorld* World, FObjectPreSaveContext) {
		// World->ExtraReferencedObjects.Remove(nullptr);
		World->ExtraReferencedObjects.RemoveAll([](UObject* Obj) { return !Obj || Obj->GetClass()->HasAnyFlags(RF_Transient); });
		World->PerModuleDataObjects.Remove(nullptr);
	});
#else
	FEditorDelegates::PreSaveWorld.AddLambda([](uint32 SaveFlags, UWorld* World) {
		// World->ExtraReferencedObjects.Remove(nullptr);
		World->ExtraReferencedObjects.RemoveAll([](UObject* Obj) { return !Obj || Obj->GetClass()->HasAnyFlags(RF_Transient); });
		World->PerModuleDataObjects.Remove(nullptr);
	});

#endif
#endif
	// 	FWorldDelegates::OnPreWorldInitialization.AddLambda([](UWorld* Wrold, const UWorld::InitializationValues IVS) { GetManager(Wrold); });
});
}  // namespace

UObject* UGenericSingletons::RegisterAsSingletonInternal(UObject* Object, const UObject* WorldContextObject, bool bReplaceExist, UClass* BaseNativeCls, UClass* BaseBPCls)
{
	check(IsValid(Object));
	UClass* InBaseClass = BaseNativeCls ? BaseNativeCls : BaseBPCls;
	if (!ensureAlwaysMsgf(!InBaseClass || Object->IsA(InBaseClass), TEXT("Object %s is not child object of %s"), *GetNameSafe(Object), *GetNameSafe(InBaseClass)))
	{
		return nullptr;
	}

	auto World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	// skip cook and CDOs without world
	if (IsGarbageCollecting() || IsRunningCommandlet() || (!World && Object->HasAnyFlags(RF_ClassDefaultObject)))
	{
		UE_LOG(LogGenericStorages, Warning, TEXT("GenericSingletons::RegisterAsSingleton Warning : %s"), *GetPathNameSafe(Object));
		return nullptr;
	}
	auto Mgr = GenericStorages::GetSingletonsManager(World);
	auto ObjCls = Object->GetClass();

	if (!bReplaceExist)
	{
		auto& Ref = Mgr->Singletons.FindOrAdd(ObjCls);
		const bool bExisted = IsValid(Ref);
		Ref = bExisted ? Ref : Object;
#if !UE_BUILD_SHIPPING
		UE_LOG(LogGenericStorages,
			   Verbose,
			   TEXT("GenericSingletons::RegisterAsSingleton %s %s(%p) -> %s(%p) -> %s(%p)"),
			   bExisted ? TEXT("Existed") : TEXT("ReplaceSingle"),
			   *GetTypedNameSafe(World),
			   World,
			   *GetTypedNameSafe(ObjCls),
			   ObjCls,
			   *GetTypedNameSafe(Ref),
			   Ref);
#endif
		return Ref;
	}

	UE_LOG(LogGenericStorages, Log, TEXT("GenericSingletons::RegisterReplacing  %s(%p) -> %s(%p) -> %s(%p)"), *GetTypedNameSafe(World), World, *GetTypedNameSafe(Object), Object, *GetTypedNameSafe(InBaseClass), InBaseClass);

	UObject* LastPtr = nullptr;
	for (auto CurClass = ObjCls; CurClass; CurClass = CurClass->GetSuperClass())
	{
		UObject*& RefPtr = Mgr->Singletons.FindOrAdd(CurClass);
		LastPtr = IsValid(RefPtr) ? RefPtr : nullptr;

		// ensureAlways(!bReplaceExist || !LastPtr || (!InBaseClass && CurClass->IsNative()) || InBaseClass /* == CurClass*/);
		// if (LastPtr && !bReplaceExist)
		// 	break;
		{
			RefPtr = Object;
#if !UE_BUILD_SHIPPING
			UE_LOG(LogGenericStorages, Log, TEXT("GenericSingletons::RegisterAsSingleton %s(%p) -> %s(%p) -> %s(%p)"), *GetTypedNameSafe(World), World, *GetTypedNameSafe(CurClass), CurClass, *GetTypedNameSafe(RefPtr), RefPtr);
#endif
		}

		if ((CurClass == InBaseClass) || (!InBaseClass && CurClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Native)))
			break;
	}
	return LastPtr;
}

bool UGenericSingletons::UnregisterSingletonImpl(UObject* Object, const UObject* WorldContextObject, UClass* InBaseClass)
{
	check(IsValid(Object));

	if (!ensureAlwaysMsgf(!InBaseClass || Object->IsA(InBaseClass), TEXT("Object %s is not child object of %s"), *GetNameSafe(Object), *GetNameSafe(InBaseClass)))
	{
		return false;
	}

	auto World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	// skip cook and CDOs without world
	if (IsGarbageCollecting() || IsRunningCommandlet() || (!World && Object->HasAnyFlags(RF_ClassDefaultObject)))
	{
		UE_LOG(LogGenericStorages, Warning, TEXT("GenericSingletons::UnregisterSingleton Warning : %s"), *GetPathNameSafe(Object));
		return false;
	}

	UE_LOG(LogGenericStorages, Log, TEXT("GenericSingletons::UnregisterSingletonImpl %s(%p) -> %s(%p) -> %s(%p)"), *GetTypedNameSafe(World), World, *GetTypedNameSafe(Object), Object, *GetTypedNameSafe(InBaseClass), InBaseClass);

	auto Mgr = GenericStorages::GetSingletonsManager(World);
	auto ObjectClass = Object->GetClass();

	UObject* LastPtr = nullptr;
	for (auto CurClass = ObjectClass; CurClass && (InBaseClass || !CurClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Native)); CurClass = CurClass->GetSuperClass())
	{
		auto Found = Mgr->Singletons.Find(CurClass);
		if (Found)
		{
			if (!LastPtr && *Found != Object)
			{
				LastPtr = IsValid(*Found) ? *Found : nullptr;
			}
			if (LastPtr)
			{
				*Found = LastPtr;
			}
			else
			{
				Mgr->Singletons.Remove(CurClass);
			}
		}

		ensureAlways((!InBaseClass && CurClass->IsNative()) || InBaseClass);
		if ((InBaseClass && CurClass == InBaseClass) || (!InBaseClass && CurClass->IsNative()))
			break;
	}
	return true;
}

UObject* UGenericSingletons::GetSingletonInternal(UClass* SubClass, const UObject* WorldContextObject, bool bCreate, UClass* BaseNativeCls, UClass* BaseBPCls)
{
	UClass* RegClass = BaseNativeCls ? BaseNativeCls : BaseBPCls;
	check(SubClass && (!RegClass || SubClass->IsChildOf(RegClass)));

	if (IsGarbageCollecting())
	{
		ensureAlwaysMsgf(!bCreate, TEXT("CreateSingleton when IsGarbageCollecting"));
		return nullptr;
	}

	if (!RegClass)
	{
		RegClass = SubClass;
	}
	else
	{
		ensureAlways(SubClass->IsChildOf(RegClass));
	}
	static auto GetValidBaseClass = [](UClass* InClass) {
		for (; InClass && !InClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Native); InClass = InClass->GetSuperClass())
		{
		}
		return InClass;
	};

	auto World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	auto Mgr = GenericStorages::GetSingletonsManager(World);
	UObject* Ptr = Mgr->Singletons.FindOrAdd(SubClass);

	if (!IsValid(Ptr) && bCreate)
	{
#if USE_GENEIRC_SINGLETON_GUARD
		GenericSingletons::FSingletonCreatationScope Scope(SubClass);
#endif
		UObject* Ctx = World;
		Ptr = GenericStorages::CreateSingletonImpl(WorldContextObject, SubClass, Ctx);
#if !UE_BUILD_SHIPPING
		UE_LOG(LogGenericStorages, Log, TEXT("GenericSingletons::CreateNewSingleton  %s(%p) -> %s(%p) -> %s(%p)"), *GetTypedNameSafe(World), World, *GetTypedNameSafe(SubClass), SubClass, *GetTypedNameSafe(Ptr), Ptr);
#endif
		if (ensureAlways(IsValid(Ptr)))
		{
			RegisterAsSingletonImpl(Ptr, WorldContextObject, true, GetValidBaseClass(SubClass));
			if (Ptr->Implements<UGenericInstanceInc>())
				IGenericInstanceInc::Execute_OnGencreicSingletonCreated(Ptr, Ctx);
		}
		else
		{
			Ptr = nullptr;
		}
	}

	return Ptr;
}

UObject* UGenericSingletons::CreateInstanceImpl(const UObject* WorldContextObject, const FObjConstructParameter& Parameter)
{
	check(Parameter.Class);
	if (!ensureAlwaysMsgf(!IsGarbageCollecting(), TEXT("CreateInstance when IsGarbageCollecting Ctx:%s Class:%s"), *GetNameSafe(WorldContextObject), *GetNameSafe(Parameter.Class)))
	{
		return nullptr;
	}

	UObject* Ptr = GenericStorages::CreateInstanceImpl(WorldContextObject, Parameter);

#if !UE_BUILD_SHIPPING
	auto World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	UE_LOG(LogGenericStorages, Log, TEXT("GenericSingletons::CreateInstanceImpl %s(%p) -> %s(%p) -> %s(%p)"), *GetTypedNameSafe(World), World, *GetTypedNameSafe(Parameter.Class), Parameter.Class, *GetTypedNameSafe(Ptr), Ptr);
#endif
	return Ptr;
}

TSharedPtr<struct FStreamableHandle> UGenericSingletons::AsyncLoadObj(const FSoftObjectPath& SoftPath, FAsyncLoadObjCallback DelegateCallback, bool bSkipInvalid, TAsyncLoadPriority Priority)
{
	auto& StreamableMgr = UAssetManager::GetStreamableManager();
	return StreamableMgr.RequestAsyncLoad(SoftPath,
										  FStreamableDelegate::CreateLambda([bSkipInvalid, Cb{MoveTemp(DelegateCallback)}, SoftPath] {
											  auto* Obj = SoftPath.ResolveObject();
											  if (!bSkipInvalid || Obj)
												  Cb.ExecuteIfBound(Obj);
										  }),
										  Priority,
										  true
#if !UE_BUILD_SHIPPING
										  ,
										  false,
										  FString::Printf(TEXT("GenericSingletons::AsyncLoadObj [%s]"), *SoftPath.ToString())
#endif
	);
}

TSharedPtr<struct FStreamableHandle> UGenericSingletons::AsyncLoadObj(const TArray<FSoftObjectPath>& InPaths, FAsyncBatchObjCallback DelegateCallback, bool bSkipInvalid, TAsyncLoadPriority Priority)
{
	auto& StreamableMgr = UAssetManager::GetStreamableManager();
	return StreamableMgr.RequestAsyncLoad(InPaths,
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
										  true
#if !UE_BUILD_SHIPPING
										  ,
										  false,
										  FString::Printf(TEXT("GenericSingletons::AsyncLoadObjs [%s]"), *FString::JoinBy(InPaths, TEXT(","), [](const FSoftObjectPath& SoftPath) { return SoftPath.ToString(); }))
#endif
	);
}
#if 0
TSharedPtr<struct FStreamableHandle> UGenericSingletons::AsyncLoadCls(const FSoftClassPath& SoftPath, FAsyncLoadClsCallback DelegateCallback, bool bSkipInvalid, TAsyncLoadPriority Priority)
{
	auto& StreamableMgr = UAssetManager::GetStreamableManager();
	return StreamableMgr.RequestAsyncLoad(SoftPath,
										  FStreamableDelegate::CreateLambda([bSkipInvalid, Cb{MoveTemp(DelegateCallback)}, SoftPath] {
											  auto* Obj = SoftPath.ResolveClass();
											  if (!bSkipInvalid || Obj)
												  Cb.ExecuteIfBound(Obj);
										  }),
										  Priority,
										  true
#if !UE_BUILD_SHIPPING
										  ,
										  false,
										  FString::Printf(TEXT("GenericSingletons::AsyncLoadCls [%s]"), *SoftPath.ToString())
#endif
	);
}

TSharedPtr<struct FStreamableHandle> UGenericSingletons::AsyncLoadCls(const TArray<FSoftClassPath>& InPaths, FAsyncBatchClsCallback DelegateCallback, bool bSkipInvalid, TAsyncLoadPriority Priority)
{
	auto& StreamableMgr = UAssetManager::GetStreamableManager();
	return StreamableMgr.RequestAsyncLoad(*reinterpret_cast<const TArray<FSoftObjectPath>*>(&InPaths),
										  FStreamableDelegate::CreateLambda([bSkipInvalid, Cb{MoveTemp(DelegateCallback)}, Paths{InPaths}] {
											  TArray<UClass*> Loaded;
											  Loaded.Reserve(Paths.Num());
											  for (int32 i = 0; i < Paths.Num(); ++i)
											  {
												  auto Cls = Paths[i].ResolveClass();
												  if (!bSkipInvalid || IsValid(Cls))
													  Loaded.Add(Cls);
											  }
											  Cb.ExecuteIfBound(Loaded);
										  }),
										  Priority,
										  true
#if !UE_BUILD_SHIPPING
										  ,
										  false,
										  FString::Printf(TEXT("GenericSingletons::AsyncLoadClss [%s]"), *FString::JoinBy(InPaths, TEXT(","), [](const FSoftObjectPath& SoftPath) { return SoftPath.ToString(); }))
#endif
	);
}
#endif
bool UGenericSingletons::AsyncCreate(const FSoftClassPath& SoftPath, FAsyncLoadObjCallback Cb, UObject* WorldContextObj)
{
	WorldContextObj = WorldContextObj ? WorldContextObj : Cb.GetUObject();
	return AsyncLoadCls(SoftPath, WorldContextObj, [WorldContextObj, Cb{MoveTemp(Cb)}](UClass* ResolvedCls) { Cb.ExecuteIfBound(CreateInstanceImpl(WorldContextObj, ResolvedCls)); }).IsValid();
}
