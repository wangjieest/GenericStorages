// Copyright GenericStorages, Inc. All Rights Reserved.
#if WITH_EDITOR

#include "UnrealEditorUtils.h"

#include "Slate.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "DataTableEditorUtils.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphUtilities.h"
#include "Engine/AssetManager.h"
#include "Engine/CollisionProfile.h"
#include "Engine/DataTable.h"
#include "Engine/ObjectLibrary.h"
#include "Engine/StreamableManager.h"
#include "Engine/UserDefinedStruct.h"
#include "Engine/World.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GameFramework/Actor.h"
#include "GenericSingletons.h"
#include "ISettingsModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "PrivateFieldAccessor.h"
#include "PropertyEditorModule.h"
#include "UObject/ObjectKey.h"
#include "UObject/TextProperty.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Notifications/SNotificationList.h"

namespace UnrealEditorUtils
{
GS_PRIVATEACCESS_STATIC_MEMBER(FEdGraphUtilities, VisualPinFactories, TArray<TSharedPtr<FGraphPanelPinFactory>>)
bool ShouldUseStructReference(UEdGraphPin* TestPin)
{
	check(TestPin);

	if (TestPin->PinType.bIsReference)
		return true;

	if (TestPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Struct)
		return false;

	static TSet<TWeakObjectPtr<UScriptStruct>> SupportedTypes = [] {
		TSet<TWeakObjectPtr<UScriptStruct>> Ret;
		Ret.Add(TBaseStructure<FLinearColor>::Get());
		Ret.Add(TBaseStructure<FVector>::Get());
		Ret.Add(TBaseStructure<FVector2D>::Get());
		Ret.Add(TBaseStructure<FRotator>::Get());
		Ret.Add(FKey::StaticStruct());
		Ret.Add(FCollisionProfileName::StaticStruct());
		return Ret;
	}();

	auto ScriptStruct = Cast<UScriptStruct>(TestPin->PinType.PinSubCategoryObject.Get());
	if (SupportedTypes.Contains(ScriptStruct))
	{
		return false;
	}

	if (!ensure(ScriptStruct->GetBoolMetaData(FBlueprintTags::BlueprintType)))
	{
		return true;
	}

	if (GIsEditor)
	{
		for (auto FactoryIt = PrivateAccessStatic::VisualPinFactories().CreateIterator(); FactoryIt; ++FactoryIt)
		{
			TSharedPtr<FGraphPanelPinFactory> FactoryPtr = *FactoryIt;
			if (FactoryPtr.IsValid())
			{
				TSharedPtr<SGraphPin> ResultVisualPin = FactoryPtr->CreatePin(TestPin);
				if (ResultVisualPin.IsValid())
				{
					SupportedTypes.Add(ScriptStruct);
					return false;
				}
			}
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
auto FindPropertyModule()
{
	return FModuleManager::LoadModulePtr<FPropertyEditorModule>(TEXT("PropertyEditor"));
}
bool UpdatePropertyViews(const TArray<UObject*>& Objects)
{
	if (FPropertyEditorModule* Module = FindPropertyModule())
	{
		Module->UpdatePropertyViews(Objects);
		return true;
	}
	return false;
}

void ReplaceViewedObjects(const TMap<UObject*, UObject*>& OldToNewObjectMap)
{
	if (FPropertyEditorModule* Module = FindPropertyModule())
	{
		Module->ReplaceViewedObjects(OldToNewObjectMap);
	}
}

void RemoveDeletedObjects(TArray<UObject*> DeletedObjects)
{
	if (FPropertyEditorModule* Module = FindPropertyModule())
	{
		Module->RemoveDeletedObjects(DeletedObjects);
	}
}

TArray<FSoftObjectPath> EditorSyncScan(UClass* Type, const FString& Dir)
{
	if (GIsEditor)
	{
		TArray<FSoftObjectPath> SoftPaths;
		if (Type && !IsRunningCommandlet())
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(FName("AssetRegistry"));
			IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
			TArray<FString> ContentPaths;
			ContentPaths.Add(Dir);
			AssetRegistry.ScanPathsSynchronous(ContentPaths);
			FARFilter Filter;
#if UE_5_00_OR_LATER
			Filter.ClassPaths.Add(FTopLevelAssetPath(Type));
#else
			Filter.ClassNames.Add(Type->GetFName());
#endif
			Filter.bRecursiveClasses = true;
			Filter.PackagePaths.Add(*Dir);
			Filter.bRecursivePaths = true;
			TArray<FAssetData> AssetList;
			AssetRegistry.GetAssets(Filter, AssetList);
			for (auto& Asset : AssetList)
				SoftPaths.Add(Asset.ToSoftObjectPath());
		}
		return SoftPaths;
	}
	return {};
}

static TSet<TWeakObjectPtr<UClass>> AsyncLoadFlags;

bool EditorAsyncLoad(UClass* Type, const FString& Dir, bool bOnce)
{
	if (GIsEditor && Type && (!bOnce || !AsyncLoadFlags.Contains(Type)) && !IsRunningCommandlet())
	{
		AsyncLoadFlags.Add(Type);
		TArray<FSoftObjectPath> SoftPaths = EditorSyncScan(Type, Dir);
		if (SoftPaths.Num() > 0)
		{
			UAssetManager::GetStreamableManager().RequestAsyncLoad(SoftPaths, FStreamableDelegate::CreateLambda([] {}), 100, true);
		}
		return true;
	}

	return false;
}

bool IdentifyPinValueOnProperty(UEdGraphPin* GraphPinObj, UObject* Obj, FProperty* Prop, bool bReset /*= false*/)
{
	if (!bReset && GraphPinObj->LinkedTo.Num() > 0)
		return false;

	FString DefaultValue;
	if (auto Func = Cast<UFunction>(Obj))
	{
		GetDefault<UEdGraphSchema_K2>()->FindFunctionParameterDefaultValue(Func, Prop, DefaultValue);
	}
	else
	{
		FBlueprintEditorUtils::PropertyValueToString(Prop, (uint8*)Obj, DefaultValue);
	}
	FString PinValueString = GraphPinObj->GetDefaultAsString();
	if (Prop->IsA<FObjectPropertyBase>())
	{
		ConstructorHelpers::StripObjectClass(DefaultValue);
		ConstructorHelpers::StripObjectClass(PinValueString);
	}

	static auto IsEmptyOrNone = [](const FString& Str) { return Str.IsEmpty() || Str == TEXT("None"); };
	if (PinValueString != DefaultValue && !(IsEmptyOrNone(PinValueString) && IsEmptyOrNone(DefaultValue)))
	{
		if (bReset)
			GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, DefaultValue);
		return true;
	}

	return false;
}

ISettingsModule* GetSettingsModule()
{
	return FModuleManager::GetModulePtr<ISettingsModule>("Settings");
}

bool RegisterSettings(const FName& ContainerName, const FName& CategoryName, const FName& SectionName, const FText& DisplayText, const FText& Description, const TWeakObjectPtr<UObject>& SettingsObject, ISettingsModule* InSettingsModule)
{
	InSettingsModule = InSettingsModule ? InSettingsModule : FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if (InSettingsModule && SettingsObject.IsValid())
	{
		InSettingsModule->RegisterSettings(ContainerName, CategoryName, SectionName, DisplayText, Description, SettingsObject);
		return true;
	}
	return false;
}

bool AddConfigurationOnProject(UObject* Obj, const FConfigurationPos& NameCfg, bool bOnlyCDO, bool bOnlyNative, bool bAllowAbstract)
{
	if (GIsEditor && Obj && (!bOnlyCDO || Obj->HasAnyFlags(RF_ClassDefaultObject)) && (!bOnlyNative || Obj->GetClass()->IsNative()) && (bAllowAbstract || !Obj->GetClass()->HasAllClassFlags(CLASS_Abstract)))
	{
		FName ExtraConfiguration = TEXT("ExtraConfiguration");
		GenericStorages::CallOnPluginReady(FSimpleDelegate::CreateWeakLambda(Obj, [=] {
			auto Name = Obj->GetName();
			Name.RemoveFromStart(TEXT("Default__"));
			FName SectionName = NameCfg.SectionName.IsNone() ? FName(*Name) : NameCfg.SectionName;
			RegisterSettings(NameCfg.ContainerName, NameCfg.CategoryName, SectionName, FText::FromName(SectionName), FText::GetEmpty(), Obj);
#if 0
			for (auto& Pair : TPropertyValueRange<FProperty>(Obj->GetClass(), Obj, EPropertyValueIteratorFlags::NoRecursion, EFieldIteratorFlags::ExcludeDeprecated))
			{
				if (!ensureMsgf(!Pair.Key->HasAnyPropertyFlags(CPF_Edit) || Pair.Key->HasAnyPropertyFlags(CPF_Config), TEXT("must set editable and config both on property : %s.%s"), *GetNameSafe(Obj->GetClass()), *Pair.Key->GetName()))
				{
					Pair.Key->SetPropertyFlags(Pair.Key->GetPropertyFlags() | CPF_Config | CPF_EditConst);
				}
			}
#endif
		}));
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

void* GetPropertyAddress(const TSharedPtr<IPropertyHandle>& PropertyHandle, UObject* Outer, void** ContainerAddr)
{
	if (!PropertyHandle.IsValid())
		return nullptr;
	if (!Outer)
	{
		TArray<UObject*> OuterObjects;
		PropertyHandle->GetOuterObjects(OuterObjects);
		if (OuterObjects.Num() != 1 || !OuterObjects[0])
			return nullptr;
		Outer = OuterObjects[0];
	}

	void* ValueAddress = nullptr;
	void* ParentAddress = nullptr;

	do
	{
		auto CurIndex = PropertyHandle->GetIndexInArray();
		auto ParentHandle = PropertyHandle->GetParentHandle();
		if (CurIndex >= 0)
		{
			auto ParentContainer = ParentHandle->GetProperty();
			bool IsOutmost = !ParentContainer || GetPropertyOwnerUObject(ParentContainer)->IsA(UClass::StaticClass());
			if (!IsOutmost)
			{
				auto ParentParentHandle = ParentHandle->GetParentHandle();
				auto ParentParentProperty = ParentParentHandle->GetProperty();

				// &Outer->[Struct...].Container[CurIndex]
				if (ensureAlways(CastField<FStructProperty>(ParentParentProperty)))
				{
					void* ParentValueAddress = GetPropertyAddress(ParentParentHandle.ToSharedRef(), Outer);
					ParentAddress = ParentParentProperty->ContainerPtrToValuePtr<void>(ParentValueAddress);
				}
			}
			else
			{
				// &Outer->Container[CurIndex]
				ParentAddress = ParentContainer->ContainerPtrToValuePtr<void>(Outer);
			}

			if (ParentHandle->AsArray().IsValid())
			{
				auto ContainerProperty = CastFieldChecked<FArrayProperty>(ParentContainer);
				if (ParentAddress)
				{
					FScriptArrayHelper ArrHelper(ContainerProperty, ParentAddress);
					ValueAddress = ArrHelper.GetRawPtr(CurIndex);
				}
				break;
			}
			if (ParentHandle->AsSet().IsValid())
			{
				auto ContainerProperty = CastFieldChecked<FSetProperty>(ParentContainer);
				if (ParentAddress)
				{
					FScriptSetHelper SetHelper(ContainerProperty, ParentAddress);
					ValueAddress = SetHelper.GetElementPtr(CurIndex);
				}
				break;
			}
			if (ParentHandle->AsMap().IsValid())
			{
				auto ContainerProperty = CastFieldChecked<FMapProperty>(ParentContainer);
				if (ParentAddress)
				{
					FScriptMapHelper MapHelper(ContainerProperty, ParentAddress);
					ValueAddress = PropertyHandle->GetKeyHandle().IsValid() ? MapHelper.GetValuePtr(CurIndex) : MapHelper.GetKeyPtr(CurIndex);
				}
				break;
			}
			ensure(false);
		}
		else
		{
			auto Property = PropertyHandle->GetProperty();
			if (ensureAlways(CastField<FStructProperty>(Property)))
			{
				bool IsOutmost = GetPropertyOwnerUObject(Property)->IsA(UClass::StaticClass());
				if (IsOutmost)
				{
					// &Outer->Value
					ValueAddress = Property->ContainerPtrToValuePtr<void>(Outer);
				}
				else
				{
					// &Outer->[Struct...].Value
					ParentAddress = GetPropertyAddress(ParentHandle.ToSharedRef(), Outer);
					ValueAddress = Property->ContainerPtrToValuePtr<void>(ParentAddress);
				}
			}
		}
	} while (0);

	if (ContainerAddr)
		*ContainerAddr = ParentAddress;

	return ValueAddress;
}

GENERICSTORAGES_API void* AccessPropertyAddress(const TSharedPtr<IPropertyHandle>& PropertyHandle)
{
	do
	{
		if (!PropertyHandle.IsValid())
			break;

		TArray<void*> RawData;
		PropertyHandle->AccessRawData(RawData);
		if (!RawData.Num())
			break;
		return RawData[0];
	} while (0);
	return nullptr;
}

void* GetPropertyMemberPtr(const TSharedPtr<IPropertyHandle>& StructPropertyHandle, FName MemberName)
{
	do
	{
		if (!StructPropertyHandle.IsValid())
			break;

		FStructProperty* StructProperty = CastField<FStructProperty>(StructPropertyHandle->GetProperty());
		if (!StructProperty)
			break;

		TArray<UObject*> OuterObjects;
		StructPropertyHandle->GetOuterObjects(OuterObjects);
		if (OuterObjects.Num() != 1)
			break;

		void* StructAddress = GetPropertyAddress(StructPropertyHandle->AsShared(), OuterObjects[0]);
		if (!StructAddress)
			break;

		auto MemberProerty = FindFProperty<FProperty>(StructProperty->Struct, MemberName);
		if (!MemberProerty)
			break;

		return MemberProerty->ContainerPtrToValuePtr<void>(StructAddress);

	} while (0);
	return nullptr;
}

void ShowNotification(const FText& Tip, bool bSucc, float FadeInDuration, float FadeOutDuration, float ExpireDuration)
{
	FNotificationInfo NotificationInfo(Tip);
	NotificationInfo.FadeInDuration = FadeInDuration;
	NotificationInfo.FadeOutDuration = FadeOutDuration;
	NotificationInfo.ExpireDuration = ExpireDuration;
	NotificationInfo.bUseLargeFont = false;

	// Add the notification to the queue
	const TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(NotificationInfo);
	Notification->SetCompletionState(bSucc ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
}

//////////////////////////////////////////////////////////////////////////
static TMap<FSoftObjectPath, TWeakObjectPtr<UScriptStruct>> TableCacheTypes;

TWeakObjectPtr<UScriptStruct> FDatatableTypePicker::FindTableType(UDataTable* Table)
{
	TWeakObjectPtr<UScriptStruct> Struct;
	if (auto Find = TableCacheTypes.Find(Table))
	{
		Struct = *Find;
	}
	return Struct;
}

TWeakObjectPtr<UScriptStruct> FDatatableTypePicker::FindTableType(TSoftObjectPtr<UDataTable> Table)
{
	return FindTableType(Table.LoadSynchronous());
}

bool FDatatableTypePicker::UpdateTableType(UDataTable* Table)
{
	if (Table)
	{
		TableCacheTypes.Emplace(FSoftObjectPath(Table), Table->RowStruct);
		return true;
	}
	return false;
}

bool FDatatableTypePicker::UpdateTableType(TSoftObjectPtr<UDataTable> Table)
{
	return UpdateTableType(Table.LoadSynchronous());
}

static TSet<FString> LoadedPaths;
bool LoadTableTypes(const FString& ScanPath)
{
	if (GIsEditor && !IsRunningCommandlet() && !LoadedPaths.Contains(ScanPath))
	{
		LoadedPaths.Add(ScanPath);
		TArray<FSoftObjectPath> SoftPaths = EditorSyncScan(UDataTable::StaticClass(), *ScanPath);
		struct FLoadScope
		{
#if UE_4_23_OR_LATER
			FLoadScope()
			{
				//if (!IsLoading())
				BeginLoad(LoadContext, TEXT("DataTableScan"));
			}
			~FLoadScope()
			{
				//if (!IsLoading())
				EndLoad(LoadContext);
			}
			TRefCountPtr<FUObjectSerializeContext> LoadContext = FUObjectThreadContext::Get().GetSerializeContext();
#else
			FLoadScope()
			{
				//if (!IsLoading())
				BeginLoad(TEXT("DataTableScan"));
			}
			~FLoadScope()
			{
				//if (!IsLoading())
				EndLoad();
			}
#endif
		};

		FLoadScope ScopeLoad;
		for (auto& Soft : SoftPaths)
		{
			auto PackageName = Soft.GetLongPackageName();
			UPackage* DependentPackage = FindObject<UPackage>(nullptr, *PackageName, true);
#if UE_5_00_OR_LATER
			FLinkerLoad* Linker = GetPackageLinker(DependentPackage, FPackagePath::FromLocalPath(*PackageName), LOAD_NoVerify | LOAD_Quiet | LOAD_DeferDependencyLoads, nullptr, nullptr);
#else
			FLinkerLoad* Linker = GetPackageLinker(DependentPackage, *PackageName, LOAD_NoVerify | LOAD_Quiet | LOAD_DeferDependencyLoads, nullptr, nullptr);
#endif
			if (Linker)
			{
				UScriptStruct* Found = nullptr;
				UScriptStruct* FirstFound = nullptr;
				for (auto& a : Linker->ImportMap)
				{
					if (a.ClassName == NAME_ScriptStruct)
					{
						auto Struct = DynamicStruct(a.ObjectName.ToString());
						static auto TableBase = FTableRowBase::StaticStruct();
						if (Struct && Struct->IsChildOf(TableBase))
						{
#if 0
							// FIXME: currently just find one type
							if (Found)
							{
								for (UScriptStruct* Type = Struct; Type && Type != TableBase; Type = Cast<UScriptStruct>(Type->GetSuperStruct()))
								{
									for (auto It = Found->Children; It; It = It->Next)
									{
										if (auto SubStruct =)
									}
								}
							}
#endif
							if (!FirstFound)
								FirstFound = Struct;
							Found = Struct;
							// break;
						}
					}
				}

				//FIXME
				if (Found && ensure(FirstFound == Found))
					TableCacheTypes.Emplace(Soft, Found);

				if (!DependentPackage)
				{
					if (auto Package = Linker->LinkerRoot)
					{
						Linker->Detach();
					}
				}
			}
		}
		return true;
	}
	return false;
}

FDatatableTypePicker::FDatatableTypePicker(TSharedPtr<IPropertyHandle> PropertyHandle)
{
	// Default Search Path
	FilterPath = TEXT("/Game/DataTables");
	Init(PropertyHandle);
}

void FDatatableTypePicker::Init(TSharedPtr<IPropertyHandle> PropertyHandle)
{
	if (PropertyHandle.IsValid())
	{
		FString DataTableType = PropertyHandle->GetMetaData("DataTableType");
		if (!DataTableType.IsEmpty())
			ScriptStruct = FindObject<UScriptStruct>(ANY_PACKAGE_COMPATIABLE, *DataTableType);
		FString DataTablePath = PropertyHandle->GetMetaData("DataTablePath");
		if (!DataTablePath.IsEmpty())
			FilterPath = DataTablePath;
		bValidate = PropertyHandle->HasMetaData("ValidateDataTable");
	}
}

TSharedRef<SWidget> FDatatableTypePicker::MakeWidget()
{
	if (ScriptStruct.IsValid())
		return SNullWidget::NullWidget;
#if 0
	RowStructs = FDataTableEditorUtils::GetPossibleStructs();
#else
	RowStructs.Reset();
	RowStructs.Add(nullptr);
	RowStructs.Append(FDataTableEditorUtils::GetPossibleStructs());
#endif
	SAssignNew(MyWidget, SComboBox<UScriptStruct*>)
		.OptionsSource(&RowStructs)
		.InitiallySelectedItem(ScriptStruct.Get())
		.OnGenerateWidget_Lambda([](UScriptStruct* InStruct) { return SNew(STextBlock).Text(InStruct ? InStruct->GetDisplayNameText() : FText::GetEmpty()); })
		.OnSelectionChanged_Lambda([this](UScriptStruct* InStruct, ESelectInfo::Type SelectInfo) {
			if (SelectInfo == ESelectInfo::OnNavigation)
			{
				return;
			}
			if (ScriptStruct != InStruct)
			{
				ScriptStruct = InStruct;
				OnChanged.ExecuteIfBound();
			}
		})[SNew(STextBlock).Text_Lambda([this]() {
			UScriptStruct* RowStruct = ScriptStruct.Get();
			return RowStruct ? RowStruct->GetDisplayNameText() : FText::GetEmpty();
		})];

	return MyWidget.ToSharedRef();
}

TSharedRef<SWidget> FDatatableTypePicker::MakePropertyWidget(TSharedPtr<IPropertyHandle> SoftHandle, bool bAllowClear)
{
	Init(SoftHandle);
	LoadTableTypes(FilterPath);
	TSharedPtr<SWidget> RetWidget;
	SAssignNew(RetWidget, SObjectPropertyEntryBox)
		.PropertyHandle(SoftHandle)
		.AllowedClass(UDataTable::StaticClass())
		.OnShouldSetAsset_Lambda([bValidate{this->bValidate}, Type{this->ScriptStruct}, FilterPath{this->FilterPath}](const FAssetData& AssetData) {
			if (!bValidate)
				return true;
			if (auto DataTable = Cast<UDataTable>(AssetData.GetAsset()))
			{
				FDatatableTypePicker::UpdateTableType(DataTable);
				if (!Type.IsValid() || DataTable->RowStruct == Type)
					return true;
			}
			return false;
		})
		.OnShouldFilterAsset_Lambda([Type{this->ScriptStruct}, FilterPath{this->FilterPath}](const FAssetData& AssetData) {
			bool bFilterOut = true;
			do
			{
				auto Path = AssetData.ToSoftObjectPath();
				if (FilterPath.IsEmpty() || Path.ToString().StartsWith(FilterPath))
				{
					auto DataTable = Cast<UDataTable>(Path.ResolveObject());
					if (DataTable)
					{
						FDatatableTypePicker::UpdateTableType(DataTable);
						if (Type.IsValid() && DataTable->RowStruct != Type)
							break;
						bFilterOut = false;
					}
					else
					{
						auto Find = TableCacheTypes.Find(Path);
						if (Find && Find->IsValid())
						{
							if (Type.IsValid() && *Find != Type)
								break;
							bFilterOut = false;
						}
						else
						{
							DataTable = Cast<UDataTable>(AssetData.GetAsset());
							if (!DataTable)
								break;
							FDatatableTypePicker::UpdateTableType(DataTable);
							if (Type.IsValid() && DataTable->RowStruct != Type)
								break;
							bFilterOut = false;
						}
					}
				}
			} while (0);
			return bFilterOut;
		})
		.AllowClear(bAllowClear)
		.DisplayThumbnail(false);
	return RetWidget.ToSharedRef();
}

TSharedRef<SWidget> FDatatableTypePicker::MakeDynamicPropertyWidget(TSharedPtr<IPropertyHandle> SoftHandle, bool bAllowClear)
{
	Init(SoftHandle);
	LoadTableTypes(FilterPath);
	TSharedPtr<SWidget> RetWidget;
	SAssignNew(RetWidget, SObjectPropertyEntryBox)
		.PropertyHandle(SoftHandle)
		.AllowedClass(UDataTable::StaticClass())
		.OnShouldSetAsset_Lambda([this](const FAssetData& AssetData) {
			if (!bValidate)
				return true;
			if (auto DataTable = Cast<UDataTable>(AssetData.GetAsset()))
			{
				FDatatableTypePicker::UpdateTableType(DataTable);
				if (!ScriptStruct.IsValid() || DataTable->RowStruct == ScriptStruct)
					return true;
			}
			return false;
		})
		.OnShouldFilterAsset_Lambda([this](const FAssetData& AssetData) {
			bool bFilterOut = true;
			do
			{
				auto Path = AssetData.ToSoftObjectPath();
				if (FilterPath.IsEmpty() || Path.ToString().StartsWith(FilterPath))
				{
					auto DataTable = Cast<UDataTable>(Path.ResolveObject());
					if (DataTable)
					{
						FDatatableTypePicker::UpdateTableType(DataTable);
						if (ScriptStruct.IsValid() && DataTable->RowStruct != ScriptStruct)
							break;
						bFilterOut = false;
					}
					else
					{
						auto Find = TableCacheTypes.Find(Path);
						if (Find && Find->IsValid())
						{
							if (ScriptStruct.IsValid() && *Find != ScriptStruct)
								break;
							bFilterOut = false;
						}
						else
						{
							DataTable = Cast<UDataTable>(AssetData.GetAsset());
							if (!DataTable)
								break;
							FDatatableTypePicker::UpdateTableType(DataTable);
							if (ScriptStruct.IsValid() && DataTable->RowStruct != ScriptStruct)
								break;
							bFilterOut = false;
						}
					}
				}
			} while (0);
			return bFilterOut;
		})
		.AllowClear(bAllowClear)
		.DisplayThumbnail(false);
	return RetWidget.ToSharedRef();
}

//////////////////////////////////////////////////////////////////////////
FScopedPropertyTransaction::FScopedPropertyTransaction(const TSharedPtr<IPropertyHandle>& InPropertyHandle, const FText& SessionName, const TCHAR* TransactionContext)
	: PropertyHandle(InPropertyHandle.Get())
	, Scoped(TransactionContext, SessionName, GetPropertyOwnerUObject(PropertyHandle->GetProperty()))
{
	PropertyHandle->NotifyPreChange();
}

FScopedPropertyTransaction::~FScopedPropertyTransaction()
{
	PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	PropertyHandle->NotifyFinishedChangingProperties();
}
bool ShouldEndPlayMap()
{
	return GEditor && GEditor->ShouldEndPlayMap();
}
}  // namespace UnrealEditorUtils
#endif
