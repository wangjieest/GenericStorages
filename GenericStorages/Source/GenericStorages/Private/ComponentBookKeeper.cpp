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

#include "ComponentBookKeeper.h"

#include "BookKeeperConfig.h"
#include "ClassDataStorage.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/MessageDialog.h"
#include "Stats/Stats2.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectMacros.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UObjectGlobals.h"
#include "GenericSingletons.h"

#if WITH_EDITOR
#	include "Editor.h"
#endif

//////////////////////////////////////////////////////////////////////////
namespace ComponentBookKeeper
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

	void RegisterAutoSpawnComponents(TSubclassOf<AActor> Class, const TSet<TSubclassOf<UActorComponent>>& RegDatas, bool bPersistent, uint8 Mode)
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

	void RegisterAutoSpawnComponent(TSubclassOf<AActor> Class, TSubclassOf<UActorComponent> RegClass, bool bPersistent, uint8 Mode)
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

void AppendDeferComponents(AActor& Actor)
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
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ComponentBookKeeper_AppendDeferComponents);
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
				UE_LOG(LogTemp, Log, TEXT("ComponentBookKeeper::AppendDeferComponents %s : %s"), *Actor.GetName(), *RegData.RegClass->GetName());
				QUICK_SCOPE_CYCLE_COUNTER(STAT_ComponentBookKeeper_CreateDataCompoents);
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
				UE_LOG(LogTemp, Warning, TEXT("ComponentBookKeeper::AppendDeferComponents Skip CompClass %s For Actor %s"), *GetNameSafe(RegData.RegClass), *GetNameSafe(&Actor));
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
}  // namespace ComponentBookKeeper

UComponentBookKeeper::UComponentBookKeeper()
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		ComponentBookKeeper::BeginListen();
	}
}

void UComponentBookKeeper::RegisterAutoSpawnComponents(TSubclassOf<AActor> Class, const TSet<TSubclassOf<UActorComponent>>& RegDatas, bool bPersistent, uint8 Mode)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ComponentBookKeeper_Registers);
	ComponentBookKeeper::Storage.RegisterAutoSpawnComponents(Class, RegDatas, bPersistent, Mode);
}

void UComponentBookKeeper::RegisterAutoSpawnComponent(TSubclassOf<AActor> Class, TSubclassOf<UActorComponent> RegClass, bool bPersistent, uint8 Mode)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ComponentBookKeeper_Register);
	ComponentBookKeeper::Storage.RegisterAutoSpawnComponent(Class, RegClass, bPersistent, Mode);
}

void UComponentBookKeeper::EnableAdd(bool bNewEnabled)
{
	ComponentBookKeeper::Storage.EnableAdd(bNewEnabled);
}
