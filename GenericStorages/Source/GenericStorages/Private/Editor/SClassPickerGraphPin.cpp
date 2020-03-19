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

#include "SClassPickerGraphPin.h"

#if WITH_EDITOR
#	include "Slate.h"

#	include "AssetRegistryModule.h"
#	include "ClassViewerFilter.h"
#	include "ClassViewerModule.h"
#	include "EdGraph/EdGraphSchema.h"
#	include "EdGraphSchema_K2.h"
#	include "Editor.h"
#	include "K2Node_CallFunction.h"
#	include "Modules/ModuleManager.h"
#	include "PropertyCustomizationHelpers.h"
#	include "ScopedTransaction.h"
#	include "UObject/Object.h"

#	define LOCTEXT_NAMESPACE "ClassPikcerGraphPin"

bool SClassPickerGraphPin::IsMatchedToCreate(UEdGraphPin* InGraphPinObj)
{
	if (IsMatchedPinType(InGraphPinObj))
	{
		return IsCustomClassPinPicker(InGraphPinObj);
	}
	return false;
}

bool SClassPickerGraphPin::IsMatchedPinType(UEdGraphPin* InGraphPinObj)
{
	return (InGraphPinObj->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct && InGraphPinObj->PinType.PinSubCategoryObject == TBaseStructure<FSoftClassPath>::Get()) || InGraphPinObj->PinType.PinCategory == UEdGraphSchema_K2::PC_SoftClass
		   || InGraphPinObj->PinType.PinCategory == UEdGraphSchema_K2::PC_Class;
}

bool SClassPickerGraphPin::IsCustomClassPinPicker(UEdGraphPin* InGraphPinObj)
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
		if (!ThisFunction || !ThisFunction->HasMetaData(TEXT("CustomClassPinPicker")))
			break;

		TArray<FString> ParameterNames;
		ThisFunction->GetMetaData(TEXT("CustomClassPinPicker")).ParseIntoArray(ParameterNames, TEXT(","), true);
		bRet = (ParameterNames.Contains(InGraphPinObj->PinName.ToString()));
	} while (0);
	return bRet;
}

bool SClassPickerGraphPin::GetMetaClassData(UEdGraphPin* InGraphPinObj, const UClass*& ImplementClass, TSet<const UClass*>& AllowedClasses)
{
	bool bAllowAbstract = false;
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

		for (UField* Child = ThisFunction->Children; Child; Child = Child->Next)
		{
			if (Child->GetFName() == InGraphPinObj->PinName)
			{
				bAllowAbstract = Child->GetBoolMetaData(TEXT("AllowAbstract"));
				ImplementClass = Child->GetClassMetaData(TEXT("MustImplement"));

				auto FilterClass = Cast<const UClass>(InGraphPinObj->PinType.PinSubCategoryObject.Get());
				auto MetaClass = Child->GetClassMetaData(TEXT("MetaClass"));
				if (!FilterClass || (MetaClass && MetaClass->IsChildOf(FilterClass)))
					FilterClass = MetaClass;

				AllowedClasses.Empty();
				if (FilterClass)
				{
					AllowedClasses.Add(FilterClass);
					break;
				}

				TArray<FString> AllowedClassNames;
				Child->GetMetaData(TEXT("AllowedClasses")).ParseIntoArray(AllowedClassNames, TEXT(","), true);
				if (AllowedClassNames.Num() > 0)
				{
					for (const FString& ClassName : AllowedClassNames)
					{
						UClass* AllowedClass = FindObject<UClass>(ANY_PACKAGE, *ClassName);
						const bool bIsInterface = AllowedClass && AllowedClass->HasAnyClassFlags(CLASS_Interface);
						if (AllowedClass && (!bIsInterface || (ImplementClass && AllowedClass->ImplementsInterface(ImplementClass))))
						{
							AllowedClasses.Add(AllowedClass);
						}
					}
				}

				break;
			}
		}

	} while (0);
	return bAllowAbstract;
}

void SClassPickerGraphPin::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
	if (InGraphPinObj->DefaultValue.IsEmpty())
		OnPickedNewClass(nullptr);
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
					GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, NewPath);
					GraphPinObj->Modify();
				}
			}
		}
	}

	return FReply::Handled();
}

class FSoftclassPathSelectorFilter : public IClassViewerFilter
{
public:
	const UPackage* GraphPinOutermostPackage = nullptr;
	TSet<const UClass*> AllowedChildrenOfClasses;
	const UClass* InterfaceThatMustBeImplemented = nullptr;
	bool bAllowAbstract = false;

	bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override
	{
		check(InClass != nullptr);
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

		return Result;
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef<const IUnloadedBlueprintData> InUnloadedClassData, TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override
	{
		bool Result = !InUnloadedClassData->HasAnyClassFlags(CLASS_Hidden | CLASS_HideDropDown | CLASS_Deprecated) && (bAllowAbstract || !InUnloadedClassData->HasAnyClassFlags(CLASS_Abstract))
					  && (InFilterFuncs->IfInChildOfClassesSet(AllowedChildrenOfClasses, InUnloadedClassData) != EFilterReturn::Failed)
					  && (!InterfaceThatMustBeImplemented || InUnloadedClassData->ImplementsInterface(InterfaceThatMustBeImplemented));

		return Result;
	}
};

TSharedRef<SWidget> SClassPickerGraphPin::GenerateAssetPicker()
{
	FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

	// Fill in options
	FClassViewerInitializationOptions Options;
	Options.Mode = EClassViewerMode::ClassPicker;
	Options.bShowNoneOption = true;

	const UClass* InterfaceMustBeImplemented;
	TSet<const UClass*> AllowedClasses;
	bool bAllowAbstract = GetMetaClassData(GraphPinObj, InterfaceMustBeImplemented, AllowedClasses);
	if (!AllowedClasses.Num())
	{
		AllowedClasses.Add(UObject::StaticClass());
	}

	TSharedPtr<FSoftclassPathSelectorFilter> Filter = MakeShareable(new FSoftclassPathSelectorFilter);
	Options.ClassFilter = Filter;

	Filter->bAllowAbstract = bAllowAbstract;
	Filter->InterfaceThatMustBeImplemented = InterfaceMustBeImplemented;
	Filter->AllowedChildrenOfClasses.Append(MoveTemp(AllowedClasses));
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
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				ClassViewerModule.CreateClassViewer(Options, FOnClassPicked::CreateSP(this, &SClassPickerGraphPin::OnPickedNewClass))
			]
		]
	];
}

FOnClicked SClassPickerGraphPin::GetOnUseButtonDelegate()
{
	return FOnClicked::CreateSP(this, &SClassPickerGraphPin::OnClickUse);
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
				GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, NewPath);
				GraphPinObj->Modify();
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

	FString CachedRuntimePath = CachedEditorAssetData.ObjectPath.ToString() + TEXT("_C");

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

			CachedEditorAssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FName(*EditorPath));

			if (!CachedEditorAssetData.IsValid())
			{
				FString PackageName = FPackageName::ObjectPathToPackageName(EditorPath);
				FString PackagePath = FPackageName::GetLongPackagePath(PackageName);
				FString ObjectName = FPackageName::ObjectPathToObjectName(EditorPath);

				// Fake one
				CachedEditorAssetData = FAssetData(FName(*PackageName), FName(*PackagePath), FName(*ObjectName), UObject::StaticClass()->GetFName());
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
		return IsCustomObjectFilter(InGraphPinObj);
	}
	return false;
}

bool SObjectFilterGraphPin::IsMatchedPinType(UEdGraphPin* InGraphPinObj)
{
	return (InGraphPinObj->PinType.PinCategory == UEdGraphSchema_K2::PC_Object);
}

bool SObjectFilterGraphPin::IsCustomObjectFilter(UEdGraphPin* InGraphPinObj)
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
		if (!ThisFunction || !ThisFunction->HasMetaData(TEXT("CustomObjectFilter")))
			break;

		TArray<FString> ParameterNames;
		ThisFunction->GetMetaData(TEXT("CustomObjectFilter")).ParseIntoArray(ParameterNames, TEXT(","), true);
		bRet = (ParameterNames.Contains(InGraphPinObj->PinName.ToString()));
	} while (0);
	return bRet;
}

bool SObjectFilterGraphPin::SetMetaClassData(UEdGraphPin* InGraphPinObj)
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

		for (UField* Child = ThisFunction->Children; Child; Child = Child->Next)
		{
			if (Child->GetFName() == InGraphPinObj->PinName)
			{
				auto MetaClass = Child->GetClassMetaData(TEXT("MetaClass"));
				auto FilterClass = Cast<const UClass>(InGraphPinObj->PinType.PinSubCategoryObject.Get());
				if (FilterClass && MetaClass && MetaClass != FilterClass && MetaClass->IsChildOf(FilterClass))
				{
					InGraphPinObj->PinType.PinSubCategoryObject = MetaClass;
					InGraphPinObj->GetOwningNode()->GetGraph()->NotifyGraphChanged();
					Ret = true;
				}
				break;
			}
		}

	} while (0);
	return Ret;
}

void SObjectFilterGraphPin::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
	SetMetaClassData(InGraphPinObj);
}
#	undef LOCTEXT_NAMESPACE

#endif