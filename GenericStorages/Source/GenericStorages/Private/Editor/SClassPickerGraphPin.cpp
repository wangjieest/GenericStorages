// Copyright GenericStorages, Inc. All Rights Reserved.

#include "SClassPickerGraphPin.h"

#include "Engine/DataTable.h"

#if WITH_EDITOR
#include "Slate.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "ClassViewerFilter.h"
#include "ClassViewerModule.h"
#include "ContentBrowserModule.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphUtilities.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "IContentBrowserSingleton.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Modules/ModuleManager.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"
#include "UObject/Object.h"
#include "Misc/EngineVersionComparison.h"
#if UE_VERSION_NEWER_THAN(5, 1, -1)
#include "Styling/AppStyle.h"
#else
#define FAppStyle FEditorStyle
#endif
#include "GraphEditAction.h"
#include "ProtectFieldAccessor.h"

#define LOCTEXT_NAMESPACE "ClassPikcerGraphPin"

bool SGraphPinObjectExtra::HasCustomMeta(UEdGraphPin* InGraphPinObj, const TCHAR* MetaName, TSet<FString>* Out)
{
	bool bRet = false;
	do
	{
		if (!InGraphPinObj)
			break;

		UK2Node_CallFunction* CallFuncNode = Cast<UK2Node_CallFunction>(InGraphPinObj->GetOwningNode());
		if (!CallFuncNode)
			break;

		const UFunction* ThisFunction = CallFuncNode->GetTargetFunction();
		if (!ThisFunction || !ThisFunction->HasMetaData(MetaName))
			break;

		TArray<FString> ParameterNames;
		ThisFunction->GetMetaData(MetaName).ParseIntoArray(ParameterNames, TEXT(","), true);
		bRet = (ParameterNames.Contains(::ToString(InGraphPinObj->PinName)));
		if (Out)
			Out->Append(ParameterNames);
	} while (0);
	return bRet;
}

FReply SGraphPinObjectExtra::OnClickUse()
{
	FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();

	UClass* ObjectClass = Cast<UClass>(GraphPinObj->PinType.PinSubCategoryObject.Get());
	if (!!ObjectClass)
	{
		UObject* SelectedObject = GEditor->GetSelectedObjects()->GetTop(ObjectClass);
		if (OnCanUseAssetData(SelectedObject))
		{
			if (!!SelectedObject)
			{
				const FScopedTransaction Transaction(LOCTEXT("ChangeObjectPinValue", "Change Object Pin Value"));
				GraphPinObj->Modify();
				GraphPinObj->GetSchema()->TrySetDefaultObject(*GraphPinObj, SelectedObject);
			}
		}
	}
	return FReply::Handled();
}

bool SGraphPinObjectExtra::OnCanUseAssetData(const FAssetData& AssetData)
{
	return true;
}

EVisibility SGraphPinObjectExtra::GetDefaultValueVisibility() const
{
	if (bHiddenDefaultValue)
		return EVisibility::Collapsed;
	else
		return SGraphPin::GetDefaultValueVisibility();
}

bool SClassPickerGraphPin::IsMatchedToCreate(UEdGraphPin* InGraphPinObj)
{
	if (IsMatchedPinType(InGraphPinObj))
	{
		return HasCustomMeta(InGraphPinObj, TEXT("CustomClassPinPicker"));
	}
	return false;
}

UClass* SClassPickerGraphPin::GetChoosenClass(UEdGraphPin* InGraphPinObj)
{
	if (InGraphPinObj->PinType.PinCategory == UEdGraphSchema_K2::PC_SoftClass || InGraphPinObj->PinType.PinCategory == UEdGraphSchema_K2::PC_Class)
		return Cast<UClass>(InGraphPinObj->DefaultObject);
	else
		return TSoftClassPtr<UObject>(InGraphPinObj->DefaultValue).LoadSynchronous();
}

bool SClassPickerGraphPin::IsMatchedPinType(UEdGraphPin* InGraphPinObj)
{
	return !InGraphPinObj->PinType.IsContainer()
		   && ((InGraphPinObj->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct && InGraphPinObj->PinType.PinSubCategoryObject == TBaseStructure<FSoftClassPath>::Get())
			   || InGraphPinObj->PinType.PinCategory == UEdGraphSchema_K2::PC_SoftClass || InGraphPinObj->PinType.PinCategory == UEdGraphSchema_K2::PC_Class);
}

struct FClassPickerGraphPinUtils
{
	static bool GetMetaClassData(UEdGraphPin* InGraphPinObj, const UClass*& ImplementClass, TSet<const UClass*>& AllowedClasses, TSet<const UClass*>& DisallowedClasses, FProperty* BindProp, TFunctionRef<void(const FProperty*)> Op)
	{
		bool bAllowAbstract = false;
		do
		{
			if (!InGraphPinObj)
				break;

			if (!ensure(BindProp || SClassPickerGraphPin::IsMatchedToCreate(InGraphPinObj)))
				break;

			if (BindProp)
			{
				Op(BindProp);

				bAllowAbstract = BindProp->GetBoolMetaData(TEXT("AllowAbstract"));
				ImplementClass = BindProp->GetClassMetaData(TEXT("MustImplement"));

				auto FilterClass = Cast<const UClass>(InGraphPinObj->PinType.PinSubCategoryObject.Get());
				auto MetaClass = BindProp->GetClassMetaData(TEXT("MetaClass"));
				if (!FilterClass || (MetaClass && MetaClass->IsChildOf(FilterClass)))
					FilterClass = MetaClass;

				AllowedClasses.Empty();
				if (FilterClass)
				{
					AllowedClasses.Add(FilterClass);
					break;
				}

				TArray<FString> AllowedClassNames;
				BindProp->GetMetaData(TEXT("AllowedClasses")).ParseIntoArray(AllowedClassNames, TEXT(","), true);
				if (AllowedClassNames.Num() > 0)
				{
					for (const FString& ClassName : AllowedClassNames)
					{
#if UE_5_00_OR_LATER
						UClass* AllowedClass = UClass::TryFindTypeSlowSafe<UClass>(ClassName);
#else
						UClass* AllowedClass = FindObject<UClass>(ANY_PACKAGE_COMPATIABLE, *ClassName);
#endif
						const bool bIsInterface = AllowedClass && AllowedClass->HasAnyClassFlags(CLASS_Interface);
						if (AllowedClass && (!bIsInterface || (ImplementClass && AllowedClass->ImplementsInterface(ImplementClass))))
						{
							AllowedClasses.Add(AllowedClass);
						}
					}
				}
				TArray<FString> DisallowedClassNames;
				BindProp->GetMetaData(TEXT("DisallowedClasses")).ParseIntoArray(DisallowedClassNames, TEXT(","), true);
				if (DisallowedClassNames.Num() > 0)
				{
					for (const FString& ClassName : DisallowedClassNames)
					{
#if UE_5_00_OR_LATER
						UClass* DisallowedClass = UClass::TryFindTypeSlowSafe<UClass>(ClassName);
#else
						UClass* DisallowedClass = FindObject<UClass>(ANY_PACKAGE_COMPATIABLE, *ClassName);
#endif
						const bool bIsInterface = DisallowedClass && DisallowedClass->HasAnyClassFlags(CLASS_Interface);
						if (DisallowedClass && (!bIsInterface || (ImplementClass && DisallowedClass->ImplementsInterface(ImplementClass))))
						{
							DisallowedClasses.Add(DisallowedClass);
						}
					}
				}
			}

			UK2Node_CallFunction* CallFuncNode = Cast<UK2Node_CallFunction>(InGraphPinObj->GetOwningNode());
			if (!CallFuncNode)
				break;

			const UFunction* ThisFunction = CallFuncNode->GetTargetFunction();
			if (!ThisFunction)
				break;

			for (TFieldIterator<FProperty> It(ThisFunction); It; ++It)
			{
				if ((It->HasAnyPropertyFlags(CPF_Parm) && It->GetFName() == ToName(InGraphPinObj->PinName)))
				{
					Op(*It);
					bAllowAbstract = It->GetBoolMetaData(TEXT("AllowAbstract"));
					ImplementClass = It->GetClassMetaData(TEXT("MustImplement"));

					auto FilterClass = Cast<const UClass>(InGraphPinObj->PinType.PinSubCategoryObject.Get());
					auto MetaClass = It->GetClassMetaData(TEXT("MetaClass"));
					if (!FilterClass || (MetaClass && MetaClass->IsChildOf(FilterClass)))
						FilterClass = MetaClass;

					AllowedClasses.Empty();
					if (FilterClass)
					{
						AllowedClasses.Add(FilterClass);
						break;
					}

					TArray<FString> AllowedClassNames;
					It->GetMetaData(TEXT("AllowedClasses")).ParseIntoArray(AllowedClassNames, TEXT(","), true);
					if (AllowedClassNames.Num() > 0)
					{
						for (const FString& ClassName : AllowedClassNames)
						{
#if UE_5_00_OR_LATER
							UClass* AllowedClass = UClass::TryFindTypeSlowSafe<UClass>(ClassName);
#else
							UClass* AllowedClass = FindObject<UClass>(ANY_PACKAGE_COMPATIABLE, *ClassName);
#endif
							const bool bIsInterface = AllowedClass && AllowedClass->HasAnyClassFlags(CLASS_Interface);
							if (AllowedClass && (!bIsInterface || (ImplementClass && AllowedClass->ImplementsInterface(ImplementClass))))
							{
								AllowedClasses.Add(AllowedClass);
							}
						}
					}
					TArray<FString> DisallowedClassNames;
					It->GetMetaData(TEXT("DisallowedClasses")).ParseIntoArray(DisallowedClassNames, TEXT(","), true);
					if (DisallowedClassNames.Num() > 0)
					{
						for (const FString& ClassName : DisallowedClassNames)
						{
#if UE_5_00_OR_LATER
							UClass* DisallowedClass = UClass::TryFindTypeSlowSafe<UClass>(ClassName);
#else
							UClass* DisallowedClass = FindObject<UClass>(ANY_PACKAGE_COMPATIABLE, *ClassName);
#endif
							const bool bIsInterface = DisallowedClass && DisallowedClass->HasAnyClassFlags(CLASS_Interface);
							if (DisallowedClass && (!bIsInterface || (ImplementClass && DisallowedClass->ImplementsInterface(ImplementClass))))
							{
								DisallowedClasses.Add(DisallowedClass);
							}
						}
					}
					break;
				}
			}

		} while (0);
		return bAllowAbstract;
	}
};

class FSoftclassPathSelectorFilter : public IClassViewerFilter
{
public:
	const UPackage* GraphPinOutermostPackage = nullptr;
	TSet<const UClass*> AllowedChildrenOfClasses;
	TSet<const UClass*> DisallowedChildrenOfClasses;
	TSet<FName> WithinMetaKeys;
	TSet<FName> WithoutMetaKeys;
	const UClass* InterfaceThatMustBeImplemented = nullptr;
	bool bAllowAbstract = false;

	bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override
	{
		check(InClass != nullptr);
		if (DisallowedChildrenOfClasses.Num() > 0 && InFilterFuncs->IfInChildOfClassesSet(DisallowedChildrenOfClasses, InClass) != EFilterReturn::Failed)
		{
			return false;
		}

		// If it appears on the allowed child-of classes list (or there is nothing on that list)
		bool Result = !InClass->HasAnyClassFlags(CLASS_Hidden | CLASS_HideDropDown | CLASS_Deprecated) && (bAllowAbstract || !InClass->HasAnyClassFlags(CLASS_Abstract))
					  && (InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InClass) != EFilterReturn::Failed) && (!InterfaceThatMustBeImplemented || InClass->ImplementsInterface(InterfaceThatMustBeImplemented));

		if (Result)
		{
			const UPackage* ClassPackage = InClass->GetOutermost();
			check(ClassPackage != nullptr);

			// Don't allow classes from a loaded map (e.g. LSBPs) unless we're already working inside that package context.
			// Otherwise, choosing the class would lead to a GLEO at save time.
			Result &= !ClassPackage->ContainsMap() || ClassPackage == GraphPinOutermostPackage;
		}

		if (Result && WithinMetaKeys.Num())
		{
			for (const FName& Key : WithinMetaKeys)
			{
				if (!InClass->HasMetaData(Key))
				{
					Result = false;
					break;
				}
			}
		}

		if (Result && WithoutMetaKeys.Num())
		{
			for (const FName& Key : WithoutMetaKeys)
			{
				if (InClass->HasMetaData(Key))
				{
					Result = false;
					break;
				}
			}
		}
		return Result;
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef<const IUnloadedBlueprintData> InUnloadedClassData, TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override
	{
		if (DisallowedChildrenOfClasses.Num() > 0 && InFilterFuncs->IfInChildOfClassesSet(DisallowedChildrenOfClasses, InUnloadedClassData) != EFilterReturn::Failed)
		{
			return false;
		}

		bool Result = !InUnloadedClassData->HasAnyClassFlags(CLASS_Hidden | CLASS_HideDropDown | CLASS_Deprecated) && (bAllowAbstract || !InUnloadedClassData->HasAnyClassFlags(CLASS_Abstract))
					  && (InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InUnloadedClassData) != EFilterReturn::Failed)
					  && (!InterfaceThatMustBeImplemented || InUnloadedClassData->ImplementsInterface(InterfaceThatMustBeImplemented));

		return Result;
	}
};

bool SClassPickerGraphPin::SetMetaInfo(UEdGraphPin* InGraphPinObj)
{
	bool bRet = false;

	do
	{
		if (!InGraphPinObj)
			break;

		if (!ensure(!bRequiredMatch || IsMatchedToCreate(InGraphPinObj)))
			break;

		if (InGraphPinObj->GetDefaultAsString().IsEmpty())
		{
			const FScopedTransaction Transaction(LOCTEXT("SetDefaultPinValue", "Set Default Pin Value"));
			if (GraphPinObj->PinType.PinCategory == UEdGraphSchema_K2::PC_SoftClass || GraphPinObj->PinType.PinCategory == UEdGraphSchema_K2::PC_Class)
				GraphPinObj->GetSchema()->TrySetDefaultObject(*GraphPinObj, nullptr);
			else
				GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, TEXT("None"));
		}

		UK2Node_CallFunction* CallFuncNode = Cast<UK2Node_CallFunction>(InGraphPinObj->GetOwningNode());
		if (!CallFuncNode)
			break;

		const UFunction* ThisFunction = CallFuncNode->GetTargetFunction();
		if (!ThisFunction)
			break;

		for (TFieldIterator<FProperty> It(ThisFunction); It; ++It)
		{
			if ((It->HasAnyPropertyFlags(CPF_Parm) && It->GetFName() == ToName(InGraphPinObj->PinName)))
			{
				if (It->HasMetaData(TEXT("NotConnectable")))
					InGraphPinObj->bNotConnectable = true;

				auto MetaClass = It->GetClassMetaData(TEXT("MetaClass"));
				auto FilterClass = Cast<const UClass>(InGraphPinObj->PinType.PinSubCategoryObject.Get());
				if (FilterClass && MetaClass && MetaClass != FilterClass && MetaClass->IsChildOf(FilterClass))
				{
					InGraphPinObj->PinType.PinSubCategoryObject = MetaClass;
					auto Node = GraphPinObj->GetOwningNode();
					RegisterActiveTimer(0.01f, FWidgetActiveTimerDelegate::CreateWeakLambda(Node, [Node](double, float) {
											Node->GetGraph()->NotifyNodeChanged(Node);
											return EActiveTimerReturnType::Stop;
										}));
					bRet = true;
				}
				break;
			}
		}

	} while (0);
	if (!ClassFilter)
	{
		const UClass* InterfaceMustBeImplemented = nullptr;
		TSet<const UClass*> AllowedClasses;
		TSet<const UClass*> DisallowedClasses;
		TSet<FName> WithinMetaKeys;
		TSet<FName> WithoutMetaKeys;
		bool bAllowAbstract = FClassPickerGraphPinUtils::GetMetaClassData(GraphPinObj, InterfaceMustBeImplemented, AllowedClasses, DisallowedClasses, BindProp, [&](const FProperty* Prop) {
			TArray<FString> WithinMetaKey;
			TArray<FString> WithoutMetaKey;
			Prop->GetMetaData(TEXT("WithinMetaKey")).ParseIntoArray(WithinMetaKey, TEXT(","), true);
			Prop->GetMetaData(TEXT("WithoutMetaKey")).ParseIntoArray(WithoutMetaKey, TEXT(","), true);
			for (auto& Key : WithinMetaKey)
			{
				WithinMetaKeys.Add(*Key);
			}
			for (auto& Key : WithoutMetaKey)
			{
				WithoutMetaKeys.Add(*Key);
			}
		});

		if (!AllowedClasses.Num())
		{
			AllowedClasses.Add(UObject::StaticClass());
		}

		TSharedPtr<FSoftclassPathSelectorFilter> Filter = MakeShareable(new FSoftclassPathSelectorFilter);
		Filter->bAllowAbstract = bAllowAbstract;
		Filter->InterfaceThatMustBeImplemented = InterfaceMustBeImplemented;
		Filter->WithinMetaKeys = MoveTemp(WithinMetaKeys);
		Filter->WithoutMetaKeys = MoveTemp(WithoutMetaKeys);
		Filter->AllowedChildrenOfClasses = MoveTemp(AllowedClasses);
		Filter->DisallowedChildrenOfClasses = MoveTemp(DisallowedClasses);
		Filter->GraphPinOutermostPackage = GraphPinObj->GetOuter()->GetOutermost();
		ClassFilter = Filter;
	}
	return bRet;
}

void SClassPickerGraphPin::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj, FProperty* InBindProp)
{
	bRequiredMatch = InArgs._bRequiredMatch;
	BindProp = InBindProp;
	auto Default = SGraphPin::FArguments();
	SGraphPin::Construct(Default, InGraphPinObj);
	SetMetaInfo(InGraphPinObj);
}

FReply SClassPickerGraphPin::OnClickUse()
{
	FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();

	if (GraphPinObj && GraphPinObj->GetSchema())
	{
		const UClass* PinRequiredParentClass = Cast<const UClass>(GraphPinObj->PinType.PinSubCategoryObject.Get());
		ensure(PinRequiredParentClass);
		const UClass* SelectedClass = GEditor->GetFirstSelectedClass(PinRequiredParentClass);
		if (SelectedClass)
		{
			// 			if (IsMatchedPinType(GraphPinObj))
			// 			{
			// 				if (GraphPinObj->DefaultObject != SelectedClass)
			// 				{
			// 					const FScopedTransaction Transaction(
			// 						NSLOCTEXT("GraphEditor", "ChangeClassPinValue", "Change Class Pin Value"));
			// 					GraphPinObj->GetSchema()->TrySetDefaultObject(*GraphPinObj, const_cast<UClass*>(SelectedClass));
			// 					GraphPinObj->Modify();
			// 				}
			// 			}
			// 			else
			{
				FString NewPath = SelectedClass ? SelectedClass->GetPathName() : TEXT("None");
				if (GraphPinObj->GetDefaultAsString() != NewPath)
				{
					const FScopedTransaction Transaction(LOCTEXT("ChangeClassPinValue", "Change Class Pin Value"));
					if (GraphPinObj->PinType.PinCategory == UEdGraphSchema_K2::PC_SoftClass || GraphPinObj->PinType.PinCategory == UEdGraphSchema_K2::PC_Class)
						GraphPinObj->GetSchema()->TrySetDefaultObject(*GraphPinObj, const_cast<UClass*>(SelectedClass));
					else
						GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, NewPath);
					FBlueprintEditorUtils::MarkBlueprintAsModified(CastChecked<UK2Node>(GraphPinObj->GetOwningNode())->GetBlueprint());
				}
			}
		}
	}

	return FReply::Handled();
}

TSharedRef<SWidget> SClassPickerGraphPin::GenerateAssetPicker()
{
	FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

	// Fill in options
	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;
	Options.bShowNoneOption = true;

	const UClass* InterfaceMustBeImplemented = nullptr;
	TSet<const UClass*> AllowedClasses;
	TSet<const UClass*> DisallowedClasses;
	TSet<FName> WithinMetaKeys;
	TSet<FName> WithoutMetaKeys;
	bool bAllowAbstract = FClassPickerGraphPinUtils::GetMetaClassData(GraphPinObj, InterfaceMustBeImplemented, AllowedClasses, DisallowedClasses, BindProp, [&](const FProperty* Prop) {
		TArray<FString> WithinMetaKey;
		TArray<FString> WithoutMetaKey;
		Prop->GetMetaData(TEXT("WithinMetaKey")).ParseIntoArray(WithinMetaKey, TEXT(","), true);
		Prop->GetMetaData(TEXT("WithoutMetaKey")).ParseIntoArray(WithoutMetaKey, TEXT(","), true);
		for (auto& Key : WithinMetaKey)
		{
			WithinMetaKeys.Add(*Key);
		}
		for (auto& Key : WithoutMetaKey)
		{
			WithoutMetaKeys.Add(*Key);
		}
	});

	if (!AllowedClasses.Num())
	{
		AllowedClasses.Add(UObject::StaticClass());
	}

	TSharedPtr<FSoftclassPathSelectorFilter> Filter = MakeShareable(new FSoftclassPathSelectorFilter);
#if UE_5_00_OR_LATER
	Options.ClassFilters.Add(Filter.ToSharedRef());
#else
	Options.ClassFilter = Filter;
#endif

	Filter->bAllowAbstract = bAllowAbstract;
	Filter->InterfaceThatMustBeImplemented = InterfaceMustBeImplemented;
	Filter->WithinMetaKeys = MoveTemp(WithinMetaKeys);
	Filter->WithoutMetaKeys = MoveTemp(WithoutMetaKeys);
	Filter->AllowedChildrenOfClasses = MoveTemp(AllowedClasses);
	Filter->DisallowedChildrenOfClasses = MoveTemp(DisallowedClasses);
	Filter->GraphPinOutermostPackage = GraphPinObj->GetOuter()->GetOutermost();

	return SNew(SBox)
			.WidthOverride(280)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.MaxHeight(500)
				[
					SNew(SBorder)
					.Padding(4)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					[
						ClassViewerModule.CreateClassViewer(Options, FOnClassPicked::CreateSP(this, &SClassPickerGraphPin::OnPickedNewClass))
					]
				]
			];
}

void SClassPickerGraphPin::OnPickedNewClass(UClass* ChosenClass)
{
	if (GraphPinObj && GraphPinObj->GetSchema())
	{
		// 		if (IsMatchedPinType(GraphPinObj))
		// 		{
		// 			if (GraphPinObj->DefaultObject != ChosenClass)
		// 			{
		// 				const FScopedTransaction Transaction(
		// 					NSLOCTEXT("GraphEditor", "ChangeClassPinValue", "Change Class Pin Value"));
		// 				GraphPinObj->GetSchema()->TrySetDefaultObject(*GraphPinObj, ChosenClass);
		// 				GraphPinObj->Modify();
		// 			}
		// 		}
		// 		else
		{
			FString NewPath = ChosenClass ? ChosenClass->GetPathName() : TEXT("None");
			if (GraphPinObj->GetDefaultAsString() != NewPath)
			{
				const FScopedTransaction Transaction(LOCTEXT("ChangeClassPinValue", "Change Class Pin Value"));
				if (GraphPinObj->PinType.PinCategory == UEdGraphSchema_K2::PC_SoftClass || GraphPinObj->PinType.PinCategory == UEdGraphSchema_K2::PC_Class)
					GraphPinObj->GetSchema()->TrySetDefaultObject(*GraphPinObj, const_cast<UClass*>(ChosenClass));
				else
					GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, NewPath);
				FBlueprintEditorUtils::MarkBlueprintAsModified(CastChecked<UK2Node>(GraphPinObj->GetOwningNode())->GetBlueprint());
			}
		}
	}
	AssetPickerAnchor->SetIsOpen(false);
}

FText SClassPickerGraphPin::GetDefaultComboText() const
{
	return LOCTEXT("DefaultComboText", "Select Class");
}

const FAssetData& SClassPickerGraphPin::GetAssetData(bool bRuntimePath) const
{
	if (bRuntimePath)
	{
		// For runtime use the default path
		return SGraphPinObject::GetAssetData(bRuntimePath);
	}

#if UE_5_00_OR_LATER
	FString CachedRuntimePath = CachedEditorAssetData.GetObjectPathString() + TEXT("_C");
#else
	FString CachedRuntimePath = CachedEditorAssetData.ObjectPath.ToString() + TEXT("_C");
#endif

	if (GraphPinObj->DefaultObject)
	{
		if (!GraphPinObj->DefaultObject->GetPathName().Equals(CachedRuntimePath, ESearchCase::CaseSensitive))
		{
			// This will cause it to use the UBlueprint
			CachedEditorAssetData = FAssetData(GraphPinObj->DefaultObject, false);
		}
	}
	else if (!GraphPinObj->DefaultValue.IsEmpty())
	{
		if (!GraphPinObj->DefaultValue.Equals(CachedRuntimePath, ESearchCase::CaseSensitive))
		{
			FString EditorPath = GraphPinObj->DefaultValue;
			EditorPath.RemoveFromEnd(TEXT("_C"));
			const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
#if UE_5_00_OR_LATER
			CachedEditorAssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(EditorPath));
#else
			CachedEditorAssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FName(*EditorPath));
#endif
			if (!CachedEditorAssetData.IsValid())
			{
				FString PackageName = FPackageName::ObjectPathToPackageName(EditorPath);
				FString PackagePath = FPackageName::GetLongPackagePath(PackageName);
				FString ObjectName = FPackageName::ObjectPathToObjectName(EditorPath);

// Fake one
#if UE_5_00_OR_LATER
				CachedEditorAssetData = FAssetData(FName(*PackageName), FName(*PackagePath), FName(*ObjectName), FTopLevelAssetPath(UObject::StaticClass()));
#else
				CachedEditorAssetData = FAssetData(FName(*PackageName), FName(*PackagePath), FName(*ObjectName), UObject::StaticClass()->GetFName());
#endif
			}
		}
	}
	else
	{
		if (CachedEditorAssetData.IsValid())
		{
			CachedEditorAssetData = FAssetData();
		}
	}

	return CachedEditorAssetData;
}

//////////////////////////////////////////////////////////////////////////
bool SObjectFilterGraphPin::IsMatchedToCreate(UEdGraphPin* InGraphPinObj)
{
	if (IsMatchedPinType(InGraphPinObj))
	{
		return HasCustomMeta(InGraphPinObj, TEXT("CustomObjectPinPicker"));
	}
	return false;
}

bool SObjectFilterGraphPin::IsMatchedPinType(UEdGraphPin* InGraphPinObj)
{
	return (InGraphPinObj->PinType.PinCategory == UEdGraphSchema_K2::PC_Object);
}

bool SObjectFilterGraphPin::SetMetaInfo(UEdGraphPin* InGraphPinObj)
{
	bool Ret = false;
	do
	{
		if (!InGraphPinObj)
			break;

		if (!ensure(IsMatchedToCreate(InGraphPinObj)))
			break;

		UK2Node_CallFunction* CallFuncNode = Cast<UK2Node_CallFunction>(InGraphPinObj->GetOwningNode());
		if (!CallFuncNode)
			break;

		const UFunction* ThisFunction = CallFuncNode->GetTargetFunction();
		if (!ThisFunction)
			break;

		for (TFieldIterator<FProperty> It(ThisFunction); It; ++It)
		{
			if ((It->HasAnyPropertyFlags(CPF_Parm) && It->GetFName() == ToName(InGraphPinObj->PinName)))
			{
				if (It->HasMetaData(TEXT("NotConnectable")))
				{
					InGraphPinObj->bNotConnectable = true;
				}
				if (It->HasMetaData(TEXT("RequireConnection")))
				{
					InGraphPinObj->bDisplayAsMutableRef = true;
					InGraphPinObj->bDefaultValueIsIgnored = true;
					bHiddenDefaultValue = true;
				}
				if (It->HasMetaData(TEXT("HiddenDefaultValue")))
				{
					InGraphPinObj->bDefaultValueIsIgnored = true;
					bHiddenDefaultValue = true;
				}
				auto MetaClass = It->GetClassMetaData(TEXT("MetaClass"));
				auto FilterClass = Cast<const UClass>(InGraphPinObj->PinType.PinSubCategoryObject.Get());
				if (FilterClass && MetaClass && MetaClass != FilterClass && MetaClass->IsChildOf(FilterClass))
				{
					InGraphPinObj->PinType.PinSubCategoryObject = MetaClass;
					WeakMetaClass = MetaClass;
					auto Node = GraphPinObj->GetOwningNode();
					RegisterActiveTimer(0.01f, FWidgetActiveTimerDelegate::CreateWeakLambda(Node, [Node](double, float) {
											Node->GetGraph()->NotifyNodeChanged(Node);
											return EActiveTimerReturnType::Stop;
										}));
					Ret = true;
				}
				break;
			}
		}

	} while (0);
	return Ret;
}

const FAssetData& SObjectFilterGraphPin::GetAssetData(bool bRuntimePath) const
{
	if (bRuntimePath)
	{
		// For runtime use the default path
		return SGraphPinObject::GetAssetData(bRuntimePath);
	}

#if UE_5_00_OR_LATER
	FString CachedRuntimePath = CachedEditorAssetData.GetObjectPathString() + TEXT("_C");
#else
	FString CachedRuntimePath = CachedEditorAssetData.ObjectPath.ToString() + TEXT("_C");
#endif

	if (GraphPinObj->DefaultObject)
	{
		if (!GraphPinObj->DefaultObject->GetPathName().Equals(CachedRuntimePath, ESearchCase::CaseSensitive))
		{
			// This will cause it to use the UBlueprint
			CachedEditorAssetData = FAssetData(GraphPinObj->DefaultObject, false);
		}
	}
	else if (!GraphPinObj->DefaultValue.IsEmpty())
	{
		if (!GraphPinObj->DefaultValue.Equals(CachedRuntimePath, ESearchCase::CaseSensitive))
		{
			FString EditorPath = GraphPinObj->DefaultValue;
			EditorPath.RemoveFromEnd(TEXT("_C"));
			const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
#if UE_5_00_OR_LATER
			CachedEditorAssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(EditorPath));
#else
			CachedEditorAssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FName(*EditorPath));
#endif
			if (!CachedEditorAssetData.IsValid())
			{
				FString PackageName = FPackageName::ObjectPathToPackageName(EditorPath);
				FString PackagePath = FPackageName::GetLongPackagePath(PackageName);
				FString ObjectName = FPackageName::ObjectPathToObjectName(EditorPath);

// Fake one
#if UE_5_00_OR_LATER
				CachedEditorAssetData = FAssetData(FName(*PackageName), FName(*PackagePath), FName(*ObjectName), FTopLevelAssetPath(UObject::StaticClass()));
#else
				CachedEditorAssetData = FAssetData(FName(*PackageName), FName(*PackagePath), FName(*ObjectName), UObject::StaticClass()->GetFName());
#endif
			}
		}
	}
	else
	{
		if (CachedEditorAssetData.IsValid())
		{
			CachedEditorAssetData = FAssetData();
		}
	}

	return CachedEditorAssetData;
}

TSharedRef<SWidget> SObjectFilterGraphPin::GenerateAssetPicker()
{
	// This class and its children are the classes that we can show objects for
	UClass* AllowedClass = Cast<UClass>(GraphPinObj->PinType.PinSubCategoryObject.Get());
	if (AllowedClass == NULL)
	{
		AllowedClass = WeakMetaClass.IsValid() ? WeakMetaClass.Get() : UObject::StaticClass();
	}

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.Filter.ClassPaths.Add(AllowedClass->GetClassPathName());
	AssetPickerConfig.bAllowNullSelection = true;
	AssetPickerConfig.Filter.bRecursiveClasses = true;
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSPLambda(this, [this](const struct FAssetData& AssetData) { OnAssetSelectedFromPicker(AssetData); });
	AssetPickerConfig.OnAssetEnterPressed = FOnAssetEnterPressed::CreateSPLambda(this, [this](const TArray<FAssetData>& InSelectedAssets) { OnAssetEnterPressedInPicker(InSelectedAssets); });

	AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
	AssetPickerConfig.bAllowDragging = false;

	// Check with the node to see if there is any "AllowClasses" or "DisallowedClasses" metadata for the pin
	FString AllowedClassesFilterString = GraphPinObj->GetOwningNode()->GetPinMetaData(GraphPinObj->PinName, FName(TEXT("AllowedClasses")));
	if (!AllowedClassesFilterString.IsEmpty())
	{
		// Clear out the allowed class names and have the pin's metadata override.
		AssetPickerConfig.Filter.ClassPaths.Empty();

		// Parse and add the classes from the metadata
		TArray<FString> AllowedClassesFilterNames;
		AllowedClassesFilterString.ParseIntoArrayWS(AllowedClassesFilterNames, TEXT(","), true);
		for (const FString& AllowedClassesFilterName : AllowedClassesFilterNames)
		{
			ensureAlwaysMsgf(!FPackageName::IsShortPackageName(AllowedClassesFilterName),
							 TEXT("Short class names are not supported as AllowedClasses on pin \"%s\": class \"%s\""),
							 *GraphPinObj->PinName.ToString(),
							 *AllowedClassesFilterName);
#if UE_5_00_OR_LATER
			AssetPickerConfig.Filter.ClassPaths.Add(FTopLevelAssetPath(AllowedClassesFilterName));
#else
			AssetPickerConfig.Filter.ClassPaths.Add(AllowedClassesFilterName);
#endif
		}
	}

	FString DisallowedClassesFilterString = GraphPinObj->GetOwningNode()->GetPinMetaData(GraphPinObj->PinName, FName(TEXT("DisallowedClasses")));
	if (!DisallowedClassesFilterString.IsEmpty())
	{
		TArray<FString> DisallowedClassesFilterNames;
		DisallowedClassesFilterString.ParseIntoArrayWS(DisallowedClassesFilterNames, TEXT(","), true);
		for (const FString& DisallowedClassesFilterName : DisallowedClassesFilterNames)
		{
			ensureAlwaysMsgf(!FPackageName::IsShortPackageName(DisallowedClassesFilterName),
							 TEXT("Short class names are not supported as DisallowedClasses on pin \"%s\": class \"%s\""),
							 *GraphPinObj->PinName.ToString(),
							 *DisallowedClassesFilterName);
#if UE_5_00_OR_LATER
			AssetPickerConfig.Filter.RecursiveClassPathsExclusionSet.Add(FTopLevelAssetPath(DisallowedClassesFilterName));
#else
			AssetPickerConfig.Filter.RecursiveClassPathsExclusionSet.Add(DisallowedClassesFilterName);
#endif
		}
	}

	return SNew(SBox)
			.HeightOverride(300)
			.WidthOverride(300)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("Menu.Background"))
				[
					ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
				]
			];
}

void SObjectFilterGraphPin::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	auto Default = SGraphPin::FArguments();
	SGraphPin::Construct(Default, InGraphPinObj);
	SetMetaInfo(InGraphPinObj);
}

//////////////////////////////////////////////////////////////////////////
bool SDataTableFilterGraphPin::IsMatchedToCreate(UEdGraphPin* InGraphPinObj)
{
	if (IsMatchedPinType(InGraphPinObj))
	{
		return HasCustomMeta(InGraphPinObj, TEXT("CustomDataTableFilter"));
	}
	return false;
}

bool SDataTableFilterGraphPin::IsMatchedPinType(UEdGraphPin* InGraphPinObj)
{
	return (InGraphPinObj->PinType.PinCategory == UEdGraphSchema_K2::PC_Object && InGraphPinObj->PinType.PinSubCategoryObject == UDataTable::StaticClass());
}

bool SDataTableFilterGraphPin::OnCanUseAssetData(const FAssetData& AssetData)
{
	if (auto DataTable = Cast<UDataTable>(AssetData.GetAsset()))
	{
		auto StructName = DataTable->RowStruct->GetName();
		if (StructNames.Contains(StructName))
			return true;
	}
	return false;
}

bool SDataTableFilterGraphPin::SetMetaInfo(UEdGraphPin* InGraphPinObj)
{
	bool bRet = false;
	do
	{
		if (!InGraphPinObj)
			break;

		if (!ensure(IsMatchedToCreate(InGraphPinObj)))
			break;

		UK2Node_CallFunction* CallFuncNode = Cast<UK2Node_CallFunction>(InGraphPinObj->GetOwningNode());
		if (!CallFuncNode)
			break;

		const UFunction* ThisFunction = CallFuncNode->GetTargetFunction();
		if (!ThisFunction)
			break;

		for (TFieldIterator<FProperty> It(ThisFunction); It; ++It)
		{
			if ((It->HasAnyPropertyFlags(CPF_Parm) && It->GetFName() == ToName(InGraphPinObj->PinName)))
			{
				if (It->HasMetaData(TEXT("NotConnectable")))
				{
					InGraphPinObj->bNotConnectable = true;
				}
				if (It->HasMetaData(TEXT("RequireConnection")))
				{
					InGraphPinObj->bDisplayAsMutableRef = true;
					InGraphPinObj->bDefaultValueIsIgnored = true;
					bHiddenDefaultValue = true;
				}
				if (It->HasMetaData(TEXT("HiddenDefaultValue")))
				{
					InGraphPinObj->bDefaultValueIsIgnored = true;
					bHiddenDefaultValue = true;
				}

				bDisableNoneSelection = It->HasMetaData(TEXT("DisableNoneSelection"));

				if (It->HasMetaData(TEXT("DataTableMetaStruct")))
				{
					TArray<FString> ParameterNames;
					It->GetMetaData(TEXT("DataTableMetaStruct")).ParseIntoArray(ParameterNames, TEXT(","), true);
					StructNames.Reset();
					StructNames.Append(ParameterNames);
					bRet = StructNames.Num() > 0;
				}
				break;
			}
		}

	} while (0);
	return bRet;
}

TSharedRef<SWidget> SDataTableFilterGraphPin::GenerateAssetPicker()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.bAllowDragging = false;
#if UE_5_00_OR_LATER
	AssetPickerConfig.Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("DataTable")));
#else
	AssetPickerConfig.Filter.ClassNames.Add(TEXT("DataTable"));
#endif
	AssetPickerConfig.OnAssetSelected = CreateWeakLambda(this, [this](const FAssetData& AssetData) { OnAssetSelectedFromPicker(AssetData); });
	AssetPickerConfig.OnAssetEnterPressed = CreateWeakLambda(this, [this](const TArray<FAssetData>& SelectedAssets) { OnAssetEnterPressedInPicker(SelectedAssets); });
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
	AssetPickerConfig.bAllowNullSelection = !bDisableNoneSelection;
	AssetPickerConfig.OnShouldFilterAsset = CreateWeakLambda(this, [this](const FAssetData& AssetData) { return !OnCanUseAssetData(AssetData); });

	return SNew(SBox)
			.HeightOverride(300)
			.WidthOverride(300)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("Menu.Background"))
				[
					ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
				]
			];
}

void SDataTableFilterGraphPin::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	auto Default = SGraphPin::FArguments();
	SGraphPin::Construct(Default, InGraphPinObj);
	SetMetaInfo(InGraphPinObj);
}

//////////////////////////////////////////////////////////////////////////
namespace CustomGraphPinPicker
{
void RegInterfaceClassSelectorFactory()
{
	class FInterfaceClassSelectorPinFactory : public FGraphPanelPinFactory
	{
		virtual TSharedPtr<SGraphPin> CreatePin(class UEdGraphPin* InPin) const override
		{
			if (SClassPickerGraphPin::IsMatchedToCreate(InPin))
			{
				return SNew(SClassPickerGraphPin, InPin);
			}
			return nullptr;
		}
	};
	FEdGraphUtilities::RegisterVisualPinFactory(MakeShareable(new FInterfaceClassSelectorPinFactory()));
}
void RegInterfaceObjectFilterFactory()
{
	class FInterfaceObjectFilterPinFactory : public FGraphPanelPinFactory
	{
		virtual TSharedPtr<SGraphPin> CreatePin(class UEdGraphPin* InPin) const override
		{
			if (SObjectFilterGraphPin::IsMatchedToCreate(InPin))
			{
				return SNew(SObjectFilterGraphPin, InPin);
			}
			return nullptr;
		}
	};
	FEdGraphUtilities::RegisterVisualPinFactory(MakeShareable(new FInterfaceObjectFilterPinFactory()));
}
void RegInterfaceDataTableFilterFactory()
{
	class FInterfaceDataTableFilterPinFactory : public FGraphPanelPinFactory
	{
		virtual TSharedPtr<SGraphPin> CreatePin(class UEdGraphPin* InPin) const override
		{
			if (SDataTableFilterGraphPin::IsMatchedToCreate(InPin))
			{
				return SNew(SDataTableFilterGraphPin, InPin);
			}
			return nullptr;
		}
	};
	FEdGraphUtilities::RegisterVisualPinFactory(MakeShareable(new FInterfaceDataTableFilterPinFactory()));
}
}  // namespace CustomGraphPinPicker
#undef LOCTEXT_NAMESPACE

#endif
