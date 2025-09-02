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
static UClass* GetValidBaseClass(UClass* InClass)
{
	while (InClass && !InClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Native))
	{
		InClass = InClass->GetSuperClass();
	}
	return InClass;
}

static UClass* TraceSingletonMeta(UClass* InCls)
{
#if WITH_EDITOR
	bool bAsSignleton = false;
	if (InCls)
	{
		for (auto Cls = InCls; Cls != UObject::StaticClass(); Cls = Cls->GetSuperClass())
		{
			if (Cls->HasMetaData(TEXT("AsSingleton")))
			{
				bAsSignleton = true;
				break;
			}
		}
	}
	if (!bAsSignleton)
	{
		UE_LOG(LogGenericStorages, Warning, TEXT("Class:{%s} as singleton which need 'AsSingleton' Meta"), *GetNameSafe(InCls));
	}
#endif
	return InCls;
}

GENERICSTORAGES_API UWorld* GetGameWorldChecked(bool bEnsureGameWorld)
{
	auto World = GWorld;
#if WITH_EDITOR
	if (!World && GIsEditor && !GIsPlayInEditorWorld)
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

			UE_LOG(LogGenericStorages, Warning, TEXT("GetGameWorldChecked Fallback to GameWorld %s"), *GetNameSafe(World));
			return World;
		};

		FWorldContext* WorldContext = GEngine->GetWorldContextFromPIEInstance(FMath::Max(0, UE::GetPlayInEditorID()));
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
			UE_LOG(LogGenericStorages, Warning, TEXT("GetGameWorldChecked Fallback to EditorWorld %s"), *GetNameSafe(World));
			World = GEditor->GetEditorWorldContext().World();
		}
	}
#else
	check(World);
#endif
	return World;
}
UGameInstance* FindGameInstance(const UObject* InObj)
{
	UGameInstance* Instance = nullptr;
#if WITH_EDITOR
	if (InObj)
	{
		if (InObj->IsA<UGameInstance>())
		{
			Instance = const_cast<UGameInstance*>(CastChecked<UGameInstance>(InObj));
		}

		if (auto World = GEngine->GetWorldFromContextObject(InObj, EGetWorldErrorMode::LogAndReturnNull))
		{
			Instance = World->GetGameInstance();
		}

		if (Instance)
			return Instance;
	}

	if (GIsEditor)
	{
		ensureAlwaysMsgf(!GIsInitialLoad && GEngine, TEXT("need to get singleton before engine initialized?"));
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
	UGameInstance* const InInstance = const_cast<UGameInstance*>(Cast<UGameInstance>(WorldContextObject));
	UWorld* const InWorld = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const bool bIsActorClass = Class->IsChildOf<AActor>();
	UObject* Ptr = nullptr;

	FName InstName = Parameter.Name;
	const bool bNameNone = Parameter.Name.IsNone();
	UObject* Ctx = const_cast<UObject*>(WorldContextObject);

	do
	{
		UGameInstance* Instance = InInstance ? InInstance : GenericStorages::FindGameInstance(WorldContextObject);
		UWorld* World = InWorld;
		if (!InWorld && IsValid(InInstance))
			World = Instance->GetWorld();
		Ctx = InInstance ? (UObject*)Instance : (UObject*)World;
#if WITH_EDITOR
		static auto GetGameInstanceClsName = [](UClass* InCls, UGameInstance* Inst) {
			auto Idx = 0;
			if (Inst)
			{
				static WorldLocalStorages::TGenericGameLocalStorage<uint64> GSIns_IndexStorage;
				Idx = ++GSIns_IndexStorage.GetLocalValue(Inst, 0);
			}
			else
			{
				static uint64 GSIns_Index = 0;
				Idx = ++GSIns_Index;
			}
			return FName(*FString::Printf(TEXT("GSIns_%s"), *GetNameSafe(InCls)), Idx);
		};
		static auto GetWorldClsName = [](UClass* InCls, UWorld* Inst) {
			static WorldLocalStorages::TGenericWorldLocalStorage<uint64> GSWorld_IndexStorage;
			auto Idx = ++GSWorld_IndexStorage.GetLocalValue(Inst, 0);
			return FName(*FString::Printf(TEXT("GSWorld_%s"), *GetNameSafe(InCls)), Idx);
		};
		if (bNameNone)
			InstName = InInstance ? GetGameInstanceClsName(Class, InInstance) : GetWorldClsName(Class, World);
#endif

#if !UE_BUILD_SHIPPING
		UE_LOG(LogGenericStorages, Log, TEXT("CreateInstanceImpl: Name:%s, Class:%s, Ctx:%s, World:%s, Instance:%s"), *InstName.ToString(), *GetNameSafe(Class), *GetNameSafe(Ctx), *GetNameSafe(InWorld), *GetNameSafe(InInstance));
#endif
		if (!Ctx)
		{
			Ptr = NewObject<UObject>(GetTransientPackage(), Class, InstName);
		}
		else if (bIsActorClass && ensureAlwaysMsgf(World, TEXT("cannot create actor while world not existed!!!")))
		{
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
#if WITH_EDITOR
			if (bNameNone)
				InstName = GetWorldClsName(Class, World);
#endif
			FActorSpawnParameters ActorSpawnParameters;
			ActorSpawnParameters.Name = InstName;
			ActorSpawnParameters.OverrideLevel = Level;
			Ptr = World->SpawnActor(Class, Parameter.Trans, ActorSpawnParameters);
		}
		else
		{
#if !UE_SERVER
			if (Class->IsChildOf<UUserWidget>())
			{
				if (InInstance)
				{
					Ptr = CreateWidget(InInstance, Class, InstName);
				}
				else
				{
					Ptr = CreateWidget(World, Class, InstName);
				}
			}
			else
#endif
			{
				Ptr = NewObject<UObject>(Ctx, Class, InstName);
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
UObject* GetSingletonCtxObject(const UObject* InObj, UWorld** OutWorld = nullptr)
{
	auto World = InObj ? InObj->GetWorld() : nullptr;
	if (OutWorld && !InObj->IsA<UGameInstance>())
	{
		InObj = World;
	}
	if (OutWorld)
		*OutWorld = World;
	return (UObject*)InObj;
}
UGenericSingletons* GetSingletonsManager(const UObject* InObj, bool bCreate)
{
	if (!ensure(!IsGarbageCollecting()))
		return nullptr;

	if (InObj && InObj->IsA<UGameInstance>())
	{
		return GenericLocalStorages::GetGameValue<UGenericSingletons>(InObj, bCreate);
	}
	return WorldLocalStorages::GetLocalValue<UGenericSingletons>(InObj, bCreate);
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

bool SetTimer(FTimerHandle& InOutHandle, UWorld* InWorld, FTimerDelegate const& InDelegate, float InRate, bool InbLoop, float InFirstDelay /*= -1.f*/)
{
	auto TimerMgr = GetTimerManager(InWorld);
	if (ensure(TimerMgr))
	{
		if (InDelegate.IsBound())
		{
			TimerMgr->SetTimer(InOutHandle, InDelegate, InRate, InbLoop, InFirstDelay);
		}
		else if (InOutHandle.IsValid())
		{
			TimerMgr->ClearTimer(InOutHandle);
		}
		return true;
	}
	return false;
}

UObject* CreateSingletonImpl(const UObject* WorldContextObject, UClass* InCls, UObject*& OutCtx)
{
	FName InstName = FName(*FString::Printf(TEXT("GSOne_%s"), *GetNameSafe(InCls)));
	auto Class = GenericStorages::TraceSingletonMeta(InCls);
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
		auto Instance = GenericStorages::FindGameInstance(InCtx);
		ensureAlwaysMsgf(GIsEditor || Instance != nullptr, TEXT("GameInstance Error"));
		if (Instance)
		{
			Instance->RegisterReferencedObject(Obj);
		}
		else
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
			if (TypeClass->IsChildOf<UEnum>())
			{
				NewReflection = StaticFindObject(TypeClass, ClassPackage, *TypeName.Mid(0, [&] {
					auto Index = TypeName.Find(TEXT("::"));
					return Index == INDEX_NONE ? MAX_int32 : Index;
				}()));
			}
			else
			{
				NewReflection = StaticFindObject(TypeClass, ClassPackage, *TypeName);
			}
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
	return GenericStorages::GetSingletonsManager(InObj, true);
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

void UGenericSingletons::PureInstanceSingleton(UClass* Class, const UObject* Ctx, UObject*& Singleton, bool& bValid, bool bCreate /*= true*/)
{
	auto Ins = GenericStorages::FindGameInstance(Ctx);
	Singleton = K2_GetSingleton(Class, Ins, bCreate);
	bValid = !!Singleton;
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

	// skip cook and CDOs
	if (IsGarbageCollecting() || IsRunningCommandlet() || Object->HasAnyFlags(RF_ClassDefaultObject))
	{
		UE_LOG(LogGenericStorages, Warning, TEXT("GenericSingletons::RegisterAsSingleton Skip : %s"), *GetPathNameSafe(Object));
		return nullptr;
	}

	UObject* SingletonCtx = GenericStorages::GetSingletonCtxObject(WorldContextObject);
	auto Mgr = GenericStorages::GetSingletonsManager(SingletonCtx, true);

	auto ObjCls = GenericStorages::TraceSingletonMeta(Object->GetClass());
#if 0
	if (!bReplaceExist)
	{
		auto& Ref = Mgr->Singletons.FindOrAdd(ObjCls);
		const bool bExisted = IsValid(Ref);
		Ref = bExisted ? Ref : Object;
#if !UE_BUILD_SHIPPING
		UE_LOG(LogGenericStorages,
			   Verbose,
			   TEXT("GenericSingletons::RegisterAsSingleton %s %s(%p) -> %s(%p) for %s(%p)"),
			   bExisted ? TEXT("Existed") : TEXT("Replaced"),
			   *GetTypedNameSafe(SingletonCtx),
			   SingletonCtx,
			   *GetTypedNameSafe(Ref),
			   Ref,
			   *GetTypedNameSafe(ObjCls),
			   ObjCls);
#endif
		return Ref;
	}
#endif

	UE_LOG(LogGenericStorages, Log, TEXT("GenericSingletons::RegisterReplacing %s(%p) -> %s(%p) for %s(%p)"), *GetTypedNameSafe(SingletonCtx), SingletonCtx, *GetTypedNameSafe(Object), Object, *GetTypedNameSafe(InBaseClass), InBaseClass);

	UObject* LastPtr = nullptr;
	for (auto CurClass = ObjCls; CurClass; CurClass = CurClass->GetSuperClass())
	{
		UObject*& RefPtr = Mgr->Singletons.FindOrAdd(CurClass);
		LastPtr = IsValid(RefPtr) ? RefPtr : nullptr;

		// ensureAlways(!bReplaceExist || !LastPtr || (!InBaseClass && CurClass->IsNative()) || InBaseClass /* == CurClass*/);
		if (LastPtr && !bReplaceExist)
			break;

		{
			RefPtr = Object;
#if !UE_BUILD_SHIPPING
			UE_LOG(LogGenericStorages,
				   Log,
				   TEXT("GenericSingletons::RegisterAsSingleton %s(%p) -> %s(%p) for %s(%p)"),
				   *GetTypedNameSafe(SingletonCtx),
				   SingletonCtx,
				   *GetTypedNameSafe(RefPtr),
				   RefPtr,
				   *GetTypedNameSafe(CurClass),
				   CurClass);
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

	// skip cook and CDOs
	if (IsGarbageCollecting() || IsRunningCommandlet() || Object->HasAnyFlags(RF_ClassDefaultObject))
	{
		UE_LOG(LogGenericStorages, Warning, TEXT("GenericSingletons::UnregisterSingleton Skip : %s"), *GetPathNameSafe(Object));
		return false;
	}

	UObject* SingletonCtx = GenericStorages::GetSingletonCtxObject(WorldContextObject);
	const bool bCreate = false;
	auto Mgr = GenericStorages::GetSingletonsManager(SingletonCtx, bCreate);
	if (!bCreate && !Mgr)
		return false;

	auto ObjectClass = Object->GetClass();
	UE_LOG(LogGenericStorages,
		   Log,
		   TEXT("GenericSingletons::UnregisterSingletonImpl %s(%p) -> %s(%p) for %s(%p)"),
		   *GetTypedNameSafe(SingletonCtx),
		   SingletonCtx,
		   *GetTypedNameSafe(Object),
		   Object,
		   *GetTypedNameSafe(InBaseClass),
		   InBaseClass);

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

	UObject* SingletonCtx = GenericStorages::GetSingletonCtxObject(WorldContextObject);
	auto Mgr = GenericStorages::GetSingletonsManager(SingletonCtx, bCreate);
	if (!bCreate && !Mgr)
		return nullptr;

	UObject*& Ptr = Mgr->Singletons.FindOrAdd(SubClass);
	if (!IsValid(Ptr) && bCreate)
	{
#if USE_GENEIRC_SINGLETON_GUARD
		GenericSingletons::FSingletonCreatationScope Scope(SubClass);
#endif
		UObject* Ctx = SingletonCtx;
		Ptr = GenericStorages::CreateSingletonImpl(WorldContextObject, SubClass, Ctx);
#if !UE_BUILD_SHIPPING
		UE_LOG(LogGenericStorages, Log, TEXT("GenericSingletons::CreateNewSingleton %s(%p) -> %s(%p) for %s(%p)"), *GetTypedNameSafe(Ctx), Ctx, *GetTypedNameSafe(Ptr), Ptr, *GetTypedNameSafe(SubClass), SubClass);
#endif
		if (ensureAlways(IsValid(Ptr)))
		{
			RegisterAsSingletonImpl(Ptr, WorldContextObject, true, GenericStorages::GetValidBaseClass(SubClass));
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
	UClass* Cls = Parameter.Class;
	if (!ensureAlwaysMsgf(!IsGarbageCollecting(), TEXT("CreateInstance when IsGarbageCollecting Ctx:%s Class:%s"), *GetNameSafe(WorldContextObject), *GetNameSafe(Cls)))
	{
		return nullptr;
	}
	auto World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	UObject* Ptr = GenericStorages::CreateInstanceImpl(WorldContextObject, Parameter);

#if !UE_BUILD_SHIPPING
	UE_LOG(LogGenericStorages, VeryVerbose, TEXT("GenericSingletons::CreateInstanceImpl %s(%p) -> %s(%p) for %s(%p)"), *GetTypedNameSafe(World), World, *GetTypedNameSafe(Ptr), Ptr, *GetTypedNameSafe(Cls), Cls);
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
TSharedPtr<struct FStreamableHandle> UGenericSingletons::AsyncLoadCls(const FSoftClassPath& InPath, FAsyncLoadClsCallback Cb, bool bSkipInvalid, TAsyncLoadPriority Priority)
{
	return AsyncLoadObj(InPath, MoveTemp(*reinterpret_cast<FAsyncLoadObjCallback*>(&Cb)), bSkipInvalid, Priority);
}

bool UGenericSingletons::AsyncCreate(const FSoftClassPath& SoftPath, FAsyncLoadObjCallback Cb, UObject* WorldContextObj)
{
	WorldContextObj = WorldContextObj ? WorldContextObj : Cb.GetUObject();
	return AsyncLoadCls(SoftPath, WorldContextObj, [WorldContextObj, Cb{MoveTemp(Cb)}](UClass* ResolvedCls) { Cb.ExecuteIfBound(CreateInstanceImpl(WorldContextObj, ResolvedCls)); }).IsValid();
}
