// Copyright GenericStorages, Inc. All Rights Reserved.
#include "GenericStoragesModule.h"

#if WITH_EDITOR
#include "Editor/ComponentPickerCustomization.h"
#include "Editor/DataTablePickerCustomization.h"
#include "Editor/GenericSystemConfigCustomization.h"
#include "Editor/SClassPickerGraphPin.h"
#include "PropertyEditorDelegates.h"
#include "PropertyEditorModule.h"

#if UE_4_25_OR_LATER
#include "Slate.h"

#include "LevelEditor.h"
#include "Settings/LevelEditorPlaySettings.h"
#endif
#endif

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GenericSingletons.h"
#include "GenericStoragesLog.h"
#include "Misc/CoreDelegates.h"
#include "Misc/DelayedAutoRegister.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "TimerManager.h"
#include "WorldLocalStorages.h"

#define LOCTEXT_NAMESPACE "GenericStoragesPlugin"

DEFINE_LOG_CATEGORY(LogGenericStorages)

namespace GenericStorages
{
static bool bModuleReady = false;
static FSimpleMulticastDelegate OnModuleStarted;
bool CallOnPluginReady(FSimpleDelegate Delegate)
{
	if (bModuleReady)
	{
		Delegate.Execute();
		return true;
	}
	else
	{
		OnModuleStarted.Add(MoveTemp(Delegate));
		return false;
	}
}

static bool bIsEngineLoopInitCompleted = false;
static FSimpleMulticastDelegate OnFEngineLoopInitCompleted;
void CallOnEngineInitCompletedImpl(FSimpleDelegate& Lambda)
{
	if (bIsEngineLoopInitCompleted)
	{
		Lambda.Execute();
	}
	else
	{
		check(IsInGameThread());
		OnFEngineLoopInitCompleted.Add(MoveTemp(Lambda));
	}
}
static FDelayedAutoRegisterHelper DelayOnEngineInitCompleted(EDelayedRegisterRunPhase::EndOfEngineInit, [] {
	bIsEngineLoopInitCompleted = true;
	OnFEngineLoopInitCompleted.Broadcast();
	OnFEngineLoopInitCompleted.Clear();
});

bool DelayExec(const UObject* InObj, FTimerDelegate Delegate, float InDelay, bool bEnsureExec)
{
	InDelay = FMath::Max(InDelay, 0.00001f);
	auto World = GEngine->GetWorldFromContextObject(InObj, InObj ? EGetWorldErrorMode::LogAndReturnNull : EGetWorldErrorMode::ReturnNull);
#if WITH_EDITOR
	if (bEnsureExec && (!World || !World->IsGameWorld()))
	{
		if (GEditor && GEditor->IsTimerManagerValid())
		{
			FTimerHandle TimerHandle;
			GEditor->GetTimerManager()->SetTimer(TimerHandle, MoveTemp(Delegate), InDelay, false);
		}
		else
		{
			auto Holder = World ? NewObject<UGenericPlaceHolder>(World) : NewObject<UGenericPlaceHolder>();
			Holder->SetFlags(RF_Standalone);
			if (World)
				World->PerModuleDataObjects.Add(Holder);
			else
				Holder->AddToRoot();

			GEditor->OnPostEditorTick().AddWeakLambda(Holder, [Holder, WeakWorld{MakeWeakObjectPtr(World)}, Delegate(MoveTemp(Delegate)), InDelay](float Delta) mutable {
				InDelay -= Delta;
				if (InDelay <= 0.f)
				{
					if (auto OwnerWorld = WeakWorld.Get())
						OwnerWorld->PerModuleDataObjects.Remove(Holder);
					Holder->RemoveFromRoot();

					Delegate.ExecuteIfBound();
					Holder->ClearFlags(RF_Standalone);
					Holder->MarkAsGarbage();
				}
			});
		}
		return true;
	}
	else
#endif
	{
		World = World ? World : GWorld;
		ensure(!bEnsureExec || World);
		if (World)
		{
			FTimerHandle TimerHandle;
			World->GetTimerManager().SetTimer(TimerHandle, MoveTemp(Delegate), InDelay, false);
			return true;
		}
	}
	return false;
}

TWeakObjectPtr<UObject> WeakPieObj;
#if WITH_EDITOR
void InitWeakPieObj()
{
	if (IsInGameThread() && TrueOnFirstCall([] {}))
	{
		FEditorDelegates::PreBeginPIE.AddLambda([](bool) {
			auto Obj = NewObject<UGenericLocalStore>();
			Obj->AddToRoot();
			GenericStorages::WeakPieObj = Obj;
		});
		FEditorDelegates::EndPIE.AddLambda([](bool) {
			auto Obj = GenericStorages::WeakPieObj.Get();
			Obj->RemoveFromRoot();
			GenericStorages::WeakPieObj = nullptr;
		});
	}
}
#endif
}  // namespace GenericStorages

UObject* UGenericLocalStore::GetPieObject(const UWorld* InCtx)
{
	checkSlow(InCtx);
#if WITH_EDITOR
	if (GIsEditor)
	{
		return GenericStorages::WeakPieObj.Get();
	}
	else
#endif
	{
		return InCtx->GetGameInstance();
	}
}

class FGenericStoragesPlugin final : public IModuleInterface
{
public:
	// IModuleInterface implementation
	virtual void StartupModule() override
	{
		GenericStorages::bModuleReady = true;
		GenericStorages::OnModuleStarted.Broadcast();
		GenericStorages::OnModuleStarted.Clear();

#if WITH_EDITOR
		GenericStorages::InitWeakPieObj();
		CustomGraphPinPicker::RegInterfaceClassSelectorFactory();
		CustomGraphPinPicker::RegInterfaceObjectFilterFactory();
		CustomGraphPinPicker::RegInterfaceDataTableFilterFactory();

		auto PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor");
		if (PropertyModule)
		{
			PropertyModule->RegisterCustomPropertyTypeLayout("DataTablePicker", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDataTablePickerCustomization::MakeInstance));
			PropertyModule->RegisterCustomPropertyTypeLayout("DataTablePathPicker", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDataTablePathPickerCustomization::MakeInstance));
			PropertyModule->RegisterCustomPropertyTypeLayout("DataTableRowNamePicker", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDataTableRowNamePickerCustomization::MakeInstance));
			PropertyModule->RegisterCustomPropertyTypeLayout("DataTableRowPicker", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDataTableRowPickerCustomization::MakeInstance));
			PropertyModule->RegisterCustomPropertyTypeLayout("ComponentPicker", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FComponentPickerCustomization::MakeInstance));
			PropertyModule->RegisterCustomPropertyTypeLayout("GenericSystemConfig", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGenericSystemConfigCustomization::MakeInstance));
			PropertyModule->NotifyCustomizationModuleChanged();
		}

#if UE_4_25_OR_LATER
		FLevelEditorModule* LevelEditorModule = GIsEditor ? FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor")) : nullptr;
		if (LevelEditorModule)
		{
			// a handy place to quick set RunUnderOneProcess
			LevelEditorModule->GetAllLevelEditorToolbarPlayMenuExtenders().Add(FLevelEditorModule::FLevelEditorMenuExtender::CreateLambda([](const TSharedRef<FUICommandList> List) {
				auto Extender = MakeShared<FExtender>();
				Extender->AddMenuExtension(TEXT("LevelEditorPlayInWindowNetwork"), EExtensionHook::After, nullptr, FMenuExtensionDelegate::CreateLambda([](FMenuBuilder& MenuBuilder) {
											   ULevelEditorPlaySettings* PlayInSettings = GetMutableDefault<ULevelEditorPlaySettings>();
											   auto CheckBox = SNew(SCheckBox)
																.IsChecked_Lambda([PlayInSettings] {
																	bool bRunUnderOneProcess = false;
																	PlayInSettings->GetRunUnderOneProcess(bRunUnderOneProcess);
																	return bRunUnderOneProcess ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
																})
																.OnCheckStateChanged_Lambda([PlayInSettings](ECheckBoxState State) {
																	PlayInSettings->SetRunUnderOneProcess(State == ECheckBoxState::Checked);
																	PlayInSettings->PostEditChange();
																	PlayInSettings->SaveConfig();
																})
																.ToolTipText(LOCTEXT("RunUnderOneProcessTip", "a handy place to quick set RunUnderOneProcess"))
																.IsFocusable(false);

											   MenuBuilder.AddWidget(CheckBox, LOCTEXT("RunUnderOneProcess", "RunUnderOneProcess"));
										   }));
				return Extender;
			}));
		}
#endif

#if PLATFORM_DESKTOP
		// auto quit ds process
		static const auto Key = TEXT("WatchParentProcess");
		if (!IsRunningCommandlet() && (FParse::Param(FCommandLine::Get(), TEXT("server")) || FParse::Param(FCommandLine::Get(), Key)))
		{
			auto CurProcId = FPlatformProcess::GetCurrentProcessId();
			FPlatformProcess::FProcEnumerator CurProcIt;
			while (CurProcIt.MoveNext())
			{
				auto CurProc = CurProcIt.GetCurrent();
				if (CurProc.GetPID() == CurProcId)
				{
#if UE_5_00_OR_LATER
					if (!CurProc.GetName().StartsWith(TEXT("UnrealEditor-")))
						break;
#else
					if (!CurProc.GetName().StartsWith(TEXT("UE4Editor-")))
						break;
#endif
					auto ParentProcHandle = FPlatformProcess::OpenProcess(CurProc.GetParentPID());
					struct FMyRunnable : public FRunnable
					{
						virtual uint32 Run() override
						{
							while (FPlatformProcess::IsProcRunning(ParentProcHandle) && !IsEngineExitRequested())
								FPlatformProcess::Sleep(1.f);

							RequestEngineExit(Key);
							return 0;
						}
						FMyRunnable(FProcHandle InHandle)
							: ParentProcHandle(InHandle)
						{
						}
						~FMyRunnable()
						{
							FPlatformProcess::CloseProc(ParentProcHandle);
							delete this;
						}
						FProcHandle ParentProcHandle;
					};

					if (FPlatformProcess::IsProcRunning(ParentProcHandle))
						FRunnableThread::Create(new FMyRunnable(ParentProcHandle), Key);
					break;
				}
			}
		}
#endif

#endif
	}

	virtual void ShutdownModule() override {}
	// End of IModuleInterface implementation
};

#if WITH_LOCALSTORAGE_MULTIMODULE_SUPPORT
namespace WorldLocalStorages
{
namespace Internal
{
	struct FStoredPair
	{
		TWeakObjectPtr<const UObject> WeakCtx;
		TMap<const char*, TSharedPtr<void>> Value;
	};
	static TArray<FStoredPair, TInlineAllocator<4>> Storages;

	void BindWorldLifetime()
	{
		if (TrueOnFirstCall([] {}))
		{
			FWorldDelegates::OnWorldCleanup.AddStatic([](UWorld* InWorld, bool, bool) { Storages.RemoveAllSwap([&](auto& Cell) { return Cell.WeakCtx == InWorld; }); });
#if WITH_EDITOR
			FEditorDelegates::EndPIE.AddStatic([](const bool) { Storages.Reset(); });
#endif
		}
	}
}  // namespace Internal

struct FStorageUtils
{
	template<typename U>
	static void* GetStorage(const U* ContextObj, const char* TypeName, const TFunctionRef<void*()>& InCtor, void (*InDtor)(void*))
	{
		Internal::BindWorldLifetime();
		check(!ContextObj || IsValid(ContextObj));
		auto& ValRef = FLocalStorageOps::FindOrAdd(Internal::Storages, ContextObj, [&]() -> TMap<const char*, TSharedPtr<void>>& {
			auto& Pair = Add_GetRef(Internal::Storages);
			Pair.WeakCtx = ContextObj;
			return Pair.Value;
		});
		auto& Ref = ValRef.FindOrAdd(TypeName);
		if (!Ref)
			Ref = MakeShareable(InCtor(), [InDtor](void* p) { InDtor(p); });
		return Ref.Get();
	}
};

void* FLocalStorageOps::GetStorageImpl(const UWorld* ContextObj, const char* TypeName, TFunctionRef<void*()> InCtor, void (*InDtor)(void*))
{
	return FStorageUtils::GetStorage(ContextObj, TypeName, InCtor, InDtor);
}
void* FLocalStorageOps::GetStorageImpl(const UGameInstance* ContextObj, const char* TypeName, TFunctionRef<void*()> InCtor, void (*InDtor)(void*))
{
	return FStorageUtils::GetStorage(ContextObj, TypeName, InCtor, InDtor);
}
void* FLocalStorageOps::GetStorageImpl(const UObject* ContextObj, const char* TypeName, TFunctionRef<void*()> InCtor, void (*InDtor)(void*))
{
	return FStorageUtils::GetStorage(ContextObj, TypeName, InCtor, InDtor);
}

}  // namespace WorldLocalStorages
#endif

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGenericStoragesPlugin, GenericStorages)
