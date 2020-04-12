// Copyright 2018-2020 wangjieest, Inc. All Rights Reserved.

#include "DeferredComponentRegistry.h"

#include "ClassDataStorage.h"
#include "DeferredComponentConfig.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GenericSingletons.h"
#include "Misc/MessageDialog.h"
#include "Stats/Stats2.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectMacros.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UObjectGlobals.h"

#if WITH_EDITOR
#	include "Editor.h"
#endif

//////////////////////////////////////////////////////////////////////////
namespace DeferredComponentRegistry
{
struct FRegClassData
{
	TSubclassOf<UActorComponent> RegClass;
	uint8 RegFlags;

	// AddUnique
	inline bool operator==(const FRegClassData& Ohter) const { return Ohter.RegClass == RegClass; }
};
using FRegClassDataArray = TArray<FRegClassData>;
struct ClassDataStorage : public ClassStorage::TClassStorageImpl<FRegClassDataArray>
{
	uint8 GetMode(uint8 InFlags, TSubclassOf<UActorComponent> InClass)
	{
		auto TmpFlags = InFlags;

		auto CDO = InClass.GetDefaultObject();
		const bool bIsReplicated = CDO->GetIsReplicated();
		const bool bIsNameStable = CDO->IsNameStableForNetworking();
		if (!TmpFlags)
		{
			if (bIsReplicated)
			{
				TmpFlags = EAttachedType::ServerSide | EAttachedType::Replicated;
				if (bIsNameStable)
					TmpFlags |= EAttachedType::NameStable | EAttachedType::ClientSide;
			}
			else
			{
				TmpFlags = EAttachedType::BothSide;
			}
		}
		else if (EAttachedType::HasAnyFlags(InFlags, EAttachedType::ServerSide))
		{
			const bool bSetReplicated = EAttachedType::HasAnyFlags(InFlags, EAttachedType::Replicated);
			const bool bSetNameStable = EAttachedType::HasAnyFlags(InFlags, EAttachedType::NameStable);
			if (bIsReplicated && !bSetReplicated)
				TmpFlags |= EAttachedType::Replicated;

			if (bIsReplicated || bSetReplicated)
			{
				if (bIsNameStable || bSetNameStable)
					TmpFlags |= EAttachedType::ClientSide | EAttachedType::NameStable;
				else
					TmpFlags &= ~EAttachedType::ClientSide | EAttachedType::NameStable;
			}
		}
		return TmpFlags;
	}

	void RegisterDeferredComponents(TSubclassOf<AActor> Class, const TSet<TSubclassOf<UActorComponent>>& RegDatas, bool bPersistent, uint8 Mode)
	{
		if (!RegDatas.Num())
			return;
		check(Class.Get());

		ModifyData(Class, bPersistent, [&](auto&& Arr) {
			for (auto& RegClass : RegDatas)
			{
				if (RegClass)
				{
					Arr.AddUnique(FRegClassData{RegClass, GetMode(Mode, RegClass)});
				}
			}
		});
	}

	void RegisterDeferredComponent(TSubclassOf<AActor> Class, TSubclassOf<UActorComponent> RegClass, bool bPersistent, uint8 Mode)
	{
		if (ensureAlways(Class.Get() && RegClass))
		{
			ModifyData(Class, bPersistent, [&](auto&& Arr) {
				auto TmpFlags = Mode;
				Arr.AddUnique(FRegClassData{RegClass, GetMode(Mode, RegClass)});
			});
		}
	}
};

ClassDataStorage Storage;

void AppendDeferredComponents(AActor& Actor)
{
#if WITH_EDITOR
	if (!Actor.GetWorld()->IsGameWorld())
		return;
#endif

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
		FString Desc = FString::Printf(TEXT("component name conflicted, prefix:%s. Class:%s Name:%s"), PrefixName, *Actor.GetClass()->GetName());

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

	auto CurClass = Actor.GetClass();
	QUICK_SCOPE_CYCLE_COUNTER(STAT_DeferredComponentRegistry_AppendDeferredComponents);
	for (auto It = Storage.CreateIterator(CurClass); It; ++It)
	{
		auto CurPtr = *It;
		for (auto& RegData : CurPtr->Data)
		{
			if (!ensure(IsValid(RegData.RegClass)))
				continue;

			const bool bIsClient = (Actor.GetNetMode() == NM_Client);
			//
			if ((RegData.RegFlags & (bIsClient ? EAttachedType::ClientSide : EAttachedType::ServerSide)) == 0)
				continue;

			auto CDO = RegData.RegClass.GetDefaultObject();
			const bool bIsReplicated = CDO->GetIsReplicated();
			const bool bSetReplicated = EAttachedType::HasAnyFlags(RegData.RegFlags, EAttachedType::Replicated);

			const bool bIsNameStable = CDO->IsNameStableForNetworking();
			const bool bSetNameStable = EAttachedType::HasAnyFlags(RegData.RegFlags, EAttachedType::NameStable);

			if (bIsClient)
			{
				if (EAttachedType::HasAnyFlags(RegData.RegFlags, EAttachedType::ServerSide) && (bIsReplicated || bSetReplicated) && !bSetNameStable && !bIsNameStable)
				{
					continue;
				}
			}

			// FIXME need this?
			if (!Actor.FindComponentByClass(CurClass))
			{
				UE_LOG(LogTemp, Log, TEXT("DeferredComponentRegistry::AppendDeferredComponents %s : %s"), *Actor.GetName(), *RegData.RegClass->GetName());
				QUICK_SCOPE_CYCLE_COUNTER(STAT_DeferredComponentRegistry_SpawnComponents);
				FName CompName = *FString::Printf(TEXT("%s_%s"), PrefixName, *RegData.RegClass->GetName());
				// no need AddOwnedComponent as NewObject does
				UActorComponent* ActorComp = NewObject<UActorComponent>(&Actor, RegData.RegClass, *CompName.ToString());
				if (bSetNameStable)
					ActorComp->SetNetAddressable();
				if (bSetReplicated)
					ActorComp->SetIsReplicated(true);
				ActorComp->RegisterComponent();

				if (TOnComponentInitialized<AActor>::bNeedInit)
				{
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
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("DeferredComponentRegistry::AppendDeferComponents Skip CompClass %s For Actor %s"), *GetNameSafe(RegData.RegClass), *GetNameSafe(&Actor));
			}
		}
	}
}

void BeginListen()
{
	if (TrueOnFirstCall([] {}))
	{
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
	}
}
}  // namespace DeferredComponentRegistry

UDeferredComponentRegistry::UDeferredComponentRegistry()
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		DeferredComponentRegistry::BeginListen();
	}
}

void UDeferredComponentRegistry::RegisterDeferredComponents(TSubclassOf<AActor> Class, const TSet<TSubclassOf<UActorComponent>>& RegDatas, bool bPersistent, uint8 Mode)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_DeferredComponentRegistry_Registers);
	DeferredComponentRegistry::Storage.RegisterDeferredComponents(Class, RegDatas, bPersistent, Mode);
}

void UDeferredComponentRegistry::RegisterDeferredComponent(TSubclassOf<AActor> Class, TSubclassOf<UActorComponent> RegClass, bool bPersistent, uint8 Mode)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_DeferredComponentRegistry_Register);
	DeferredComponentRegistry::Storage.RegisterDeferredComponent(Class, RegClass, bPersistent, Mode);
}

void UDeferredComponentRegistry::EnableAdd(bool bNewEnabled)
{
	DeferredComponentRegistry::Storage.EnableAdd(bNewEnabled);
}
