// Copyright GenericStorages, Inc. All Rights Reserved.

#include "GenericSystems.h"

#include "ClassDataStorage.h"
#include "Components/ActorComponent.h"
#include "DeferredComponentRegistry.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Internationalization/Text.h"
#include "Misc/MessageDialog.h"
#include "PrivateFieldAccessor.h"
#include "Stats/Stats2.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"
#include "WorldLocalStorages.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Editor/UnrealEditorUtils.h"
#include "EditorLevelUtils.h"
#endif

#define LOCTEXT_NAMESPACE "GenericSystems"

bool FGenericSystemConfig::operator==(const FGenericSystemConfig& rhs) const
{
	return rhs.SystemClass == SystemClass;
}

bool FGenericSystemConfig::operator==(const UClass* rhs) const
{
	return !(SystemClass || rhs) || rhs->IsChildOf(SystemClass);
}

//////////////////////////////////////////////////////////////////////////
namespace GenericSystems
{
void EditorSetActorFolderPath(AActor* Actor, FName FolderName = NAME_None)
{
#if WITH_EDITOR
	if (GIsEditor && Actor && !Actor->HasAnyFlags(RF_ClassDefaultObject) && Actor->GetWorld() && !Actor->GetWorld()->IsPreviewWorld() && (!Actor->GetFolderPath().IsValid() || Actor->GetFolderPath().IsNone()))
		Actor->SetFolderPath(FolderName == NAME_None ? FName("0.GenericSystems") : FolderName);
#endif
}

#if WITH_EDITOR
GS_PRIVATEACCESS_MEMBER(AActor, ActorLabel, FString)
#endif
void EditorSetActorLabelName(AActor* Actor, const FString& LabelName = {})
{
#if WITH_EDITOR
	if (GIsEditor && Actor && !Actor->HasAnyFlags(RF_ClassDefaultObject) && Actor->GetWorld() && !Actor->GetWorld()->IsPreviewWorld() && PrivateAccess::ActorLabel(*Actor).IsEmpty())
	{
		Actor->SetActorLabel(LabelName.IsEmpty() ? Actor->GetClass()->GetName() : LabelName, false);
	}
#endif
}
}  // namespace GenericSystems

AGenericSystemMgr::AGenericSystemMgr()
{
#if UE_4_24_OR_LATER
	SetCanBeDamaged(false);
	SetHidden(true);
	SetReplicateMovement(false);
#else
	bCanBeDamaged = false;
	bHidden = true;
	bReplicateMovement = false;
#endif

#if WITH_EDITORONLY_DATA
	bActorLabelEditable = false;
#endif
	PrimaryActorTick.bCanEverTick = false;
}

AGenericSystemMgr* AGenericSystemMgr::Get(ULevel* InLevel, bool bCreate)
{
	if (!InLevel || !InLevel->IsPersistentLevel())
		return nullptr;

	if (bCreate)
	{
		for (AActor* Actor : InLevel->Actors)
		{
			if (auto SysMgr = Cast<AGenericSystemMgr>(Actor))
			{
				UGenericSingletons::RegisterAsSingleton(SysMgr, InLevel->GetWorld(), true);
				return SysMgr;
			}
		}

		return UGenericSingletons::TryGetSingleton<AGenericSystemMgr>(InLevel->GetWorld(), [&] {
			FActorSpawnParameters ActorSpawnParameters;
			ActorSpawnParameters.Name = FName(".SystemManager");
			ActorSpawnParameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
			ActorSpawnParameters.OverrideLevel = InLevel;
			//ActorSpawnParameters.ObjectFlags = RF_Transient;
			ActorSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto SysMgr = InLevel->GetWorld()->SpawnActor<AGenericSystemMgr>(ActorSpawnParameters);
			GenericSystems::EditorSetActorLabelName(SysMgr);
			GenericSystems::EditorSetActorFolderPath(SysMgr);
			return SysMgr;
		});
	}
	else
	{
		auto SysMgr = UGenericSingletons::GetSingleton<AGenericSystemMgr>(InLevel->GetWorld(), false);
		if (SysMgr && !ensure(SysMgr->GetLevel() == InLevel))
			SysMgr = nullptr;
		return SysMgr;
	}
}

#if WITH_EDITOR
void AGenericSystemMgr::AddSystemInstance(AGenericSystemBase* Ins)
{
	RefreshImpl(EPropertyChangeType::Unspecified);
}

void AGenericSystemMgr::DelSystemInstance(AGenericSystemBase* Ins)
{
	auto Index = SystemSettings.IndexOfByKey(Ins->GetClass());
	if (Index != INDEX_NONE)
		SystemSettings.RemoveAt(Index);
	Modify();
	RefreshImpl(EPropertyChangeType::Unspecified);
}

void AGenericSystemMgr::PostLoad()
{
	Super::PostLoad();
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		GenericSystems::EditorSetActorFolderPath(this);
		GenericSystems::EditorSetActorLabelName(this);
		Refresh();
	}
}

void AGenericSystemMgr::PostActorCreated()
{
	Super::PostActorCreated();
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		GenericSystems::EditorSetActorFolderPath(this);
		GenericSystems::EditorSetActorLabelName(this);
		Refresh();
	}
}

void AGenericSystemMgr::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged)
	{
		const FName PropertyName = PropertyThatChanged->GetFName();
		if (PropertyName == GET_MEMBER_NAME_CHECKED(AGenericSystemMgr, SystemSettings))
		{
			switch (PropertyChangedEvent.ChangeType)
			{
				case EPropertyChangeType::ArrayRemove:
				case EPropertyChangeType::ArrayClear:
					ClearUseless();
					break;
				default:
					RefreshImpl(PropertyChangedEvent.ChangeType, true);
					break;
			}
		}
	}
}

void AGenericSystemMgr::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
	const FName MemberPropertyName = PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetValue()->GetFName();

	if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(AGenericSystemMgr, SystemSettings))
	{
		switch (PropertyChangedEvent.ChangeType)
		{
			case EPropertyChangeType::ValueSet:
				RefreshImpl(PropertyChangedEvent.ChangeType, true);
				break;
			default:
				break;
		}
	}
}

void AGenericSystemMgr::Refresh()
{
	RefreshImpl(EPropertyChangeType::Unspecified, true);
}

void AGenericSystemMgr::RefreshImpl(uint32 ChangeType, bool bNotify)
{
	if (!GIsEditor || !GetWorld() || GetWorld()->IsGameWorld() || GetLevel() != GetWorld()->PersistentLevel || IsRunningCommandlet() || !GetOutermost()->ContainsMap())
		return;

	GEditor->BeginTransaction(TEXT("AGenericSystemMgr"), LOCTEXT("RefreshImpl", "RefreshImpl"), this);
	bool bContainsNone = ChangeType == EPropertyChangeType::ArrayAdd;
	bool bMarkPackageDirty = (ChangeType == EPropertyChangeType::ArrayAdd || ChangeType == EPropertyChangeType::ArrayAdd || ChangeType == EPropertyChangeType::ArrayClear);

	TArray<AGenericSystemBase*> ValidInstances;
	TArray<TWeakObjectPtr<AGenericSystemBase>> InvalidInstances;
	for (AActor* Actor : GetLevel()->Actors)
	{
		auto Sys = Cast<AGenericSystemBase>(Actor);
		if (IsValid(Sys) && !Sys->GetName().StartsWith(TEXT("REINST_")))
		{
			auto Find = ValidInstances.FindByPredicate([&](auto Ins) { return Ins->IsA(Sys->GetClass()); });
			Find = Find ? Find : &Add_GetRef(ValidInstances);
			*Find = Sys;
		}
		else
		{
			InvalidInstances.Add(Sys);
		}
	}

	TArray<FGenericSystemConfig> ValidSettings;
	for (auto It = SystemSettings.CreateIterator(); It; ++It)
	{
		if (!It->SystemClass || !It->SystemClass->IsChildOf<AGenericSystemBase>() || ValidSettings.FindByKey(It->SystemClass))
		{
			bMarkPackageDirty = true;
			bContainsNone = bContainsNone || (!It->SystemClass || !It->SystemClass->IsChildOf<AGenericSystemBase>());
		}
		else
		{
			auto Find = ValidInstances.FindByPredicate([&](auto Ins) { return Ins->IsA(It->SystemClass); });
			if (!Find)
			{
				Find = &Add_GetRef(ValidInstances);

				FActorSpawnParameters ActorSpawnParameters;
				ActorSpawnParameters.OverrideLevel = GetLevel();
				ActorSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				auto Sys = GetWorld()->SpawnActor<AGenericSystemBase>(It->SystemClass, ActorSpawnParameters);
				*Find = Sys;
				bMarkPackageDirty = true;
			}
			if (ensure(*Find))
			{
				ValidSettings.Add(FGenericSystemConfig{(*Find)->GetClass(), *Find});
			}
		}
	}

	TArray<AGenericSystemBase*> ArrToDel;
	for (AActor* Actor : GetLevel()->Actors)
	{
		if (auto Sys = Cast<AGenericSystemBase>(Actor))
		{
			if (!InvalidInstances.Contains(Sys) && !ValidInstances.Contains(Sys))
			{
				ArrToDel.Add(Sys);
			}
		}
	}

	for (auto It = ArrToDel.CreateIterator(); It; ++It)
	{
		GetWorld()->EditorDestroyActor(*It, false);
		bMarkPackageDirty = true;
	}

	for (AActor* Actor : GetLevel()->Actors)
	{
		if (auto Sys = Cast<AGenericSystemBase>(Actor))
		{
			if (!InvalidInstances.Contains(Sys))
			{
				ValidSettings.AddUnique(FGenericSystemConfig{Sys->GetClass(), Sys});
			}
		}
	}

	SystemSettings = MoveTemp(ValidSettings);
	if (bContainsNone)
		SystemSettings.AddDefaulted();

	if (bMarkPackageDirty)
		MarkPackageDirty();
	GEditor->EndTransaction();

	UnrealEditorUtils::UpdatePropertyViews({this});
}

void AGenericSystemMgr::ClearUseless()
{
	if (!GIsEditor || !GetWorld() || GetWorld()->IsGameWorld() || GetLevel() != GetWorld()->PersistentLevel || IsRunningCommandlet())
		return;

	GEditor->BeginTransaction(TEXT("AGenericSystemMgr"), LOCTEXT("CleanSystemsImpl", "CleanSystemsImpl"), GetLevel());
	bool bMarkPackageDirty = false;

	bool bContainsNone = false;
	TArray<AGenericSystemBase*> ValidInstances;
	for (AActor* Actor : GetLevel()->Actors)
	{
		auto Sys = Cast<AGenericSystemBase>(Actor);
		if (IsValid(Sys))
		{
			auto Find = ValidInstances.FindByPredicate([&](auto Ins) { return Ins->IsA(Sys->GetClass()); });
			Find = (Find && SystemSettings.FindByKey((*Find)->GetClass())) ? Find : &Add_GetRef(ValidInstances);
			*Find = Sys;
		}
	}

	TArray<FGenericSystemConfig> ValidSettings;
	for (auto It = SystemSettings.CreateIterator(); It; ++It)
	{
		if (!It->SystemClass || !It->SystemClass->IsChildOf<AGenericSystemBase>()  //
			|| ValidSettings.FindByKey(It->SystemClass) || !ValidInstances.FindByPredicate([&](auto Ins) { return Ins->IsA(It->SystemClass); }))
		{
			bMarkPackageDirty = true;
			bContainsNone = bContainsNone || (!It->SystemClass || !It->SystemClass->IsChildOf<AGenericSystemBase>());
		}
		else
		{
			Add_GetRef(ValidSettings, *It);
		}
	}

	TArray<AGenericSystemBase*> InsToDel;
	for (AActor* Actor : GetLevel()->Actors)
	{
		auto Sys = Cast<AGenericSystemBase>(Actor);
		if (IsValid(Sys))
		{
			auto Find = ValidSettings.FindByKey(Sys->GetClass());
			if (!Find || !ValidInstances.Contains(Sys))
			{
				InsToDel.Add(Sys);
			}
			else
			{
				Find->SystemClass = Sys->GetClass();
				Find->SystemIns = Sys;
			}
		}
	}

	for (auto It = InsToDel.CreateIterator(); It; ++It)
	{
		GetWorld()->EditorDestroyActor(*It, false);
		bMarkPackageDirty = true;
	}

	SystemSettings = MoveTemp(ValidSettings);
	if (bContainsNone)
		SystemSettings.AddDefaulted();

	if (bMarkPackageDirty)
		MarkPackageDirty();
	GEditor->EndTransaction();
	UnrealEditorUtils::UpdatePropertyViews({this});
}
#endif

//////////////////////////////////////////////////////////////////////////
AGenericSystemBase::AGenericSystemBase()
{
#if UE_4_24_OR_LATER
	SetCanBeDamaged(false);
	SetHidden(true);
	SetReplicateMovement(false);
#else
	bCanBeDamaged = false;
	bHidden = true;
	bReplicateMovement = false;
#endif
#if WITH_EDITORONLY_DATA
	bActorLabelEditable = false;
#endif
	NetPriority = 0.8f;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = true;
}

void AGenericSystemBase::PostInitProperties()
{
	Super::PostInitProperties();
}

void AGenericSystemBase::RegisterToWorld(ESystemSource Src)
{
#if WITH_EDITOR
	if (GIsEditor && GetWorld() && !GetOutermost()->ContainsMap() && !GIsDuplicatingClassForReinstancing)
		return;

	static WorldLocalStorages::TGenericWorldLocalStorage<FTimerHandle> TimerHandle;
	GetWorld()->GetTimerManager().SetTimer(TimerHandle.GetLocalValue(GetWorld()),
										   FTimerDelegate::CreateWeakLambda(this,
																			[this] {
																				GenericSystems::EditorSetActorLabelName(this, GetClass()->GetName());
																				if (auto SysMgr = GIsEditor ? AGenericSystemMgr::Get(GetLevel()) : (AGenericSystemMgr*)nullptr)
																				{
																					AttachToActor(SysMgr, FAttachmentTransformRules::KeepRelativeTransform);
																					SysMgr->AddSystemInstance(this);
																					GenericSystems::EditorSetActorFolderPath(this, SysMgr->GetFolderPath());
																					if (GEngine)
																					{
																						GEngine->OnLevelActorFolderChanged().Add(CreateWeakLambda(this, [this](const AActor* Actor, FName OldName) {
																							if (auto Mgr = AGenericSystemMgr::Get(GetLevel()))
																								GenericSystems::EditorSetActorFolderPath(this, Mgr->GetFolderPath());
																						}));
																					}
																					SysMgr->Modify();
																				}
																				else
																				{
																					GenericSystems::EditorSetActorFolderPath(this);
																				}
																			}),
										   0.1f,
										   false);

#else
	if (!GetWorld() || !GetWorld()->IsGameWorld())
		return;
#endif

	if (DeferredComponents.Num() && !ensure(!GetWorld()->GetBegunPlay()))
	{
#if WITH_EDITOR
		static WorldLocalStorages::TGenericWorldLocalStorage<bool> StaticHasWarned;
		static auto TestIfWarned = [](bool& v) {
			auto old = v;
			v = true;
			return !old;
		};
		if (TestIfWarned(StaticHasWarned.GetLocalValue(this, false)))
#endif
		{
			FString DebugMsg = FString::Printf(TEXT("system [%s] must add deferred component class before world's beginplay!!!"), *GetPathName());
#if WITH_EDITOR && !UE_SERVER
			if (GIsEditor)
				GEditor->GetTimerManager()->SetTimerForNextTick([DebugMsg] { FMessageDialog::Debugf(FText::FromString(DebugMsg)); });
#else
			ensureAlwaysMsgf(false, TEXT("%s"), *DebugMsg);
#endif
		}
		return;
	}

	auto Last = UGenericSingletons::RegisterAsSingletonImpl(this, this, false);
	if (Last && !Last->GetClass()->HasAnyClassFlags(CLASS_NewerVersionExists) && (Last != this))
	{
		FString DebugMsg = FString::Printf(TEXT("System Conflict:%s<->%s"), *GetNameSafe(Last), *GetName());
#if WITH_EDITOR
		if (GIsEditor)
			GEditor->GetTimerManager()->SetTimerForNextTick([DebugMsg] { FMessageDialog::Debugf(FText::FromString(DebugMsg)); });
#else
		ensureAlwaysMsgf(GIsEditor || Last == this, TEXT("%s"), *DebugMsg);
#endif
		return;
	}

	// if (GetLevel() != GetWorld()->PersistentLevel)
	{
		UDeferredComponentRegistry::EnableAdd(true);
		struct FRegInfo
		{
			TSubclassOf<UActorComponent> Class;
			uint8 DeferredFlag;
			bool operator==(const FRegInfo& Othter) const { return Class == Othter.Class; }
		};

		TMap<TSubclassOf<AActor>, TArray<FRegInfo>> SortedMap;
		SortedMap.Reserve(DeferredComponents.Num());

		for (auto& RegInfo : DeferredComponents)
		{
			if (ensure(RegInfo.ActorClass && RegInfo.DeferredComponent))
				SortedMap.FindOrAdd(RegInfo.ActorClass).AddUnique(FRegInfo{RegInfo.DeferredComponent, RegInfo.DeferredFlag});
		}

		for (auto& Pair : SortedMap)
		{
			UDeferredComponentRegistry::ModifyDeferredComponents(Pair.Key, [&](auto& Arr) {
				for (auto& RegInfo : Pair.Value)
					Arr.AddUnique(DeferredComponentRegistry::FRegClassData{RegInfo.Class, RegInfo.DeferredFlag});
			});
		}
		PostRegistered();
	}
}

void AGenericSystemBase::PostActorCreated()
{
	Super::PostActorCreated();
	RegisterToWorld(ESystemSource::DynamicSpawn);
}

void AGenericSystemBase::Destroyed()
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		if (auto SysMgr = AGenericSystemMgr::Get(GetLevel(), false))
		{
			SysMgr->DelSystemInstance(this);
		}
	}
#endif
	Super::Destroyed();
}

void AGenericSystemBase::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
#if 0
	if (bDuplicateForPIE)
		RegisterToWorld(ESystemSource::Duplicate);
#endif
}

void AGenericSystemBase::PostLoad()
{
	Super::PostLoad();
	if (!HasAnyFlags(RF_ClassDefaultObject) && !IsRunningCommandlet())
		RegisterToWorld(ESystemSource::LevelLoad);
}

bool AGenericSystemBase::IsNameStableForNetworking() const
{
	return Super::IsNameStableForNetworking();
}
#undef LOCTEXT_NAMESPACE
