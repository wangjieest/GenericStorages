// Copyright GenericStorages, Inc. All Rights Reserved.

#include "DeferredComponentRegistry.h"

#include "ClassDataStorage.h"
#include "DeferredComponentConfig.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GenericSingletons.h"
#include "GenericStoragesLog.h"
#include "Misc/MessageDialog.h"
#include "Stats/Stats2.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectMacros.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UObjectGlobals.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

//////////////////////////////////////////////////////////////////////////
namespace DeferredComponentRegistry
{
struct ClassDataStorage : public ClassStorage::TClassStorageImpl<FRegClassDataArray>
{
	void AddDeferredComponents(TSubclassOf<AActor> Class, const TSet<TSubclassOf<UActorComponent>>& RegDatas, bool bPersistent, uint8 Mode)
	{
		if (!RegDatas.Num())
			return;
		check(Class.Get());

		ModifyData(Class, bPersistent, [&](auto&& Arr) {
			for (auto& RegClass : RegDatas)
			{
				if (RegClass)
				{
					Arr.AddUnique(FRegClassData{RegClass, UDeferredComponentRegistry::GetMode(Mode, RegClass)});
				}
			}
		});
	}

	void AddDeferredComponent(TSubclassOf<AActor> Class, TSubclassOf<UActorComponent> RegClass, bool bPersistent, uint8 Mode)
	{
		if (ensureAlways(Class.Get() && RegClass))
		{
			ModifyData(Class, bPersistent, [&](auto&& Arr) {
				auto TmpFlags = Mode;
				Arr.AddUnique(FRegClassData{RegClass, UDeferredComponentRegistry::GetMode(Mode, RegClass)});
			});
		}
	}
};

ClassDataStorage Storage;

void AppendDeferredComponents(AActor& Actor)
{
#if WITH_EDITOR
	auto World = Actor.GetWorld();
	if (!(World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Game))
		return;
#endif
	auto CurClass = Actor.GetClass();
	auto It = Storage.CreateIterator(CurClass);
	if (!It)
		return;

	static auto PrefixName = TEXT("`");
#if WITH_EDITOR
	// check names
	TArray<FString> Names;
	for (auto& a : Actor.GetComponents())
	{
		if (a->GetName().StartsWith(PrefixName))
			Names.Add(a->GetName());
	}
	if (Names.Num())
	{
		FString Desc = FString::Printf(TEXT("component name conflicted:%s."), *Actor.GetClass()->GetName());

		for (auto& a : Names)
		{
			Desc += TEXT("\n\t-->") + a;
		}
		if (FPlatformMisc::IsDebuggerPresent())
		{
			ensureAlwaysMsgf(false, TEXT("%s"), *Desc);
		}
		else
		{
			FMessageDialog::Debugf(FText::FromString(Desc));
		}
	}
#endif

	QUICK_SCOPE_CYCLE_COUNTER(STAT_DeferredComponentRegistry_AppendDeferredComponents);
	do
	{
		auto& CurData = *It;
		for (auto& RegData : CurData.Data)
		{
			if (!ensure(IsValid(RegData.RegClass)))
				continue;

			const bool bIsClient = (Actor.GetNetMode() != NM_DedicatedServer);
			//
			if ((RegData.RegFlags & (bIsClient ? EComponentDeferredMode::ClientSide : EComponentDeferredMode::ServerSide)) == 0)
				continue;

			auto CDO = RegData.RegClass.GetDefaultObject();
			const bool bIsReplicated = CDO->GetIsReplicated();
			const bool bSetReplicated = EComponentDeferredMode::HasAnyFlags(RegData.RegFlags, EComponentDeferredMode::Replicated);

			const bool bIsNameStable = CDO->IsNameStableForNetworking();
			const bool bSetNameStable = EComponentDeferredMode::HasAnyFlags(RegData.RegFlags, EComponentDeferredMode::NameStable);

			if (bIsClient)
			{
				if (EComponentDeferredMode::HasAnyFlags(RegData.RegFlags, EComponentDeferredMode::ServerSide) && (bIsReplicated || bSetReplicated) && !bSetNameStable && !bIsNameStable)
				{
					continue;
				}
			}

			// check existing deferred component
			if (!ensure(!Actor.FindComponentByClass(CurClass)))
			{
				UE_LOG(LogGenericStorages, Warning, TEXT("DeferredComponentRegistry::AppendDeferComponents Skip ActorComponent %s For Actor %s"), *GetNameSafe(RegData.RegClass), *GetNameSafe(&Actor));
			}
			else
			{
				UE_LOG(LogGenericStorages, Log, TEXT("DeferredComponentRegistry::AppendDeferredComponents %s : %s"), *Actor.GetName(), *RegData.RegClass->GetName());
				QUICK_SCOPE_CYCLE_COUNTER(STAT_DeferredComponentRegistry_SpawnComponents);
				FName CompName = *FString::Printf(TEXT("%s_%s"), PrefixName, *RegData.RegClass->GetName());
				// no need AddOwnedComponent as NewObject does
				UActorComponent* ActorComp = NewObject<UActorComponent>(&Actor, RegData.RegClass, *CompName.ToString(), RF_Transient);
				if (bSetNameStable)
					ActorComp->SetNetAddressable();
				if (bSetReplicated)
					ActorComp->SetIsReplicated(true);
				ActorComp->RegisterComponent();

				if (TOnComponentInitialized<AActor>::bNeedInit)
				{
					Actor.AddInstanceComponent(ActorComp);

					if (ActorComp->bAutoActivate && !ActorComp->IsActive())
					{
						ActorComp->Activate(true);
					}
					if (ActorComp->bWantsInitializeComponent && !ActorComp->HasBeenInitialized())
					{
						ActorComp->InitializeComponent();
					}
				}
			}
		}
	} while (++It);
}

static FDelayedAutoRegisterHelper DelayInnerInitUDeferredComponentRegistry(EDelayedRegisterRunPhase::EndOfEngineInit, [] {
#if WITH_EDITOR
	if (GIsEditor)
	{
		FEditorDelegates::PreBeginPIE.AddLambda([](bool bIsSimulating) { Storage.Cleanup(); });
	}
	else
#endif
	{
		// cleanup
		// Register for PreloadMap so cleanup can occur on map transitions
		FCoreUObjectDelegates::PreLoadMap.AddLambda([](const FString& MapName) { Storage.Cleanup(); });
		// FWorldDelegates::OnPreWorldFinishDestroy.AddLambda([](UWorld*) { Storage.Cleanup(); });
	}

	// Bind
	TOnComponentInitialized<AActor>::Bind();
});
}  // namespace DeferredComponentRegistry

void UDeferredComponentRegistry::AddDeferredComponents(TSubclassOf<AActor> Class, const TSet<TSubclassOf<UActorComponent>>& RegDatas, bool bPersistent, uint8 Mode)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_DeferredComponentRegistry_Registers);
	DeferredComponentRegistry::Storage.AddDeferredComponents(Class, RegDatas, bPersistent, Mode);
}

void UDeferredComponentRegistry::ModifyDeferredComponents(TSubclassOf<AActor> Class, TFunctionRef<void(DeferredComponentRegistry::FRegClassDataArray&)> Cb, bool bPersistent)
{
	DeferredComponentRegistry::Storage.ModifyData(Class, bPersistent, Cb);
}

void UDeferredComponentRegistry::AddDeferredComponent(TSubclassOf<AActor> Class, TSubclassOf<UActorComponent> RegClass, bool bPersistent, uint8 Mode)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_DeferredComponentRegistry_Register);
	DeferredComponentRegistry::Storage.AddDeferredComponent(Class, RegClass, bPersistent, Mode);
}

void UDeferredComponentRegistry::EnableAdd(bool bNewEnabled)
{
	DeferredComponentRegistry::Storage.SetEnableState(bNewEnabled);
}

uint8 UDeferredComponentRegistry::GetMode(uint8 InFlags, TSubclassOf<UActorComponent> InClass)
{
	auto TmpFlags = InFlags;
	auto CDO = InClass.GetDefaultObject();
	if (!TmpFlags)
	{
		if (CDO->GetIsReplicated())
		{
			const bool bIsNameStable = CDO->IsNameStableForNetworking();
			TmpFlags = EComponentDeferredMode::ServerSide | EComponentDeferredMode::Replicated;
			if (bIsNameStable)
				TmpFlags |= EComponentDeferredMode::NameStable | EComponentDeferredMode::ClientSide;
		}
		else
		{
			TmpFlags = EComponentDeferredMode::BothSide;
		}
	}
	else if (EComponentDeferredMode::HasAnyFlags(InFlags, EComponentDeferredMode::ServerSide))
	{
		bool bSetReplicated = EComponentDeferredMode::HasAnyFlags(InFlags, EComponentDeferredMode::Replicated);
		const bool bSetNameStable = EComponentDeferredMode::HasAnyFlags(InFlags, EComponentDeferredMode::NameStable);
		if (!bSetReplicated && CDO->GetIsReplicated())
		{
			TmpFlags |= EComponentDeferredMode::Replicated;
			bSetReplicated = true;
		}

		if (bSetReplicated)
		{
			if (bSetNameStable || CDO->IsNameStableForNetworking())
				TmpFlags |= EComponentDeferredMode::ClientSide | EComponentDeferredMode::NameStable;
			else
				TmpFlags &= ~EComponentDeferredMode::ClientSide | EComponentDeferredMode::NameStable;
		}
	}
	return TmpFlags;
}
