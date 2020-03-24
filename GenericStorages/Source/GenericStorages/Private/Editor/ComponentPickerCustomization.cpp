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

#include "ComponentPickerCustomization.h"
#if WITH_EDITOR
#	include "Slate.h"

#	include "AssetData.h"
#	include "DetailCategoryBuilder.h"
#	include "DetailLayoutBuilder.h"
#	include "Editor.h"
#	include "Engine/BlueprintGeneratedClass.h"
#	include "Engine/SCS_Node.h"
#	include "Engine/SimpleConstructionScript.h"
#	include "GameFramework/Actor.h"
#	include "IDetailChildrenBuilder.h"
#	include "IPropertyTypeCustomization.h"
#	include "IPropertyUtilities.h"
#	include "Internationalization/Text.h"
#	include "Misc/AssertionMacros.h"
#	include "Modules/ModuleManager.h"
#	include "PropertyCustomizationHelpers.h"
#	include "PropertyEditorModule.h"
#	include "PropertyHandle.h"
#	include "UObject/UnrealType.h"
#	include "UnrealEditorUtils.h"
#	include "Widgets/Text/STextBlock.h"

namespace ComponentPicker
{
static const FName NameComponent = TEXT("ComponentName");
static const FName NameComponentClass = TEXT("ComponentClass");
}  // namespace ComponentPicker

TSharedRef<IPropertyTypeCustomization> FComponentPickerCustomization::MakeInstance()
{
	return MakeShareable(new FComponentPickerCustomization);
}

void FComponentPickerCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);
	if (OuterObjects.Num() == 1)
	{
		RootPropertyHandle = PropertyHandle;
		auto Outer = OuterObjects[0];
		AActor* Actor = nullptr;
		auto ComponentHandle = PropertyHandle->GetChildHandle(ComponentPicker::NameComponent, false);
		check(ComponentHandle.IsValid());
		ComponentHandle->MarkHiddenByCustomization();

		auto FilterHandle = PropertyHandle->GetChildHandle(ComponentPicker::NameComponentClass, false);
		check(FilterHandle.IsValid());

		FName SelfComponentName;
		auto TestOuter = Outer;
		while (TestOuter)
		{
			Actor = Cast<AActor>(TestOuter);
			if (Actor)
				break;
			if (SelfComponentName.IsNone())
			{
				if (auto SelfComp = Cast<UActorComponent>(TestOuter))
				{
					SelfComponentName = SelfComp->GetFName();
				}
			}
			TestOuter = TestOuter->GetOuter();
		}

		FilterClass = UActorComponent::StaticClass();
		const FString& ClassName = PropertyHandle->GetMetaData("MetaClass");
		if (!ClassName.IsEmpty())
		{
			UClass* Class = FindObject<UClass>(ANY_PACKAGE, *ClassName);
			if (!Class)
			{
				Class = LoadObject<UClass>(nullptr, *ClassName);
			}
			if (Class && Class->IsChildOf<UActorComponent>())
			{
				FilterClass = Class;
			}
		}
		else
		{
			if (auto Class = UnrealEditorUtils::GetPropertyMemberPtr<TSubclassOf<UActorComponent>>(PropertyHandle, ComponentPicker::NameComponentClass))
			{
				if (Class->Get())
				{
					FilterClass = *Class;
				}
			}
		}

		TSubclassOf<AActor> CDOActorClass = nullptr;
		if (!Actor || !Actor->GetLevel())
		{
			TestOuter = Outer;
			while (TestOuter)
			{
				UBlueprintGeneratedClass* RootBlueprintGeneratedClass = Cast<UBlueprintGeneratedClass>(TestOuter->GetClass());
				if (RootBlueprintGeneratedClass && RootBlueprintGeneratedClass->IsChildOf(AActor::StaticClass()))
				{
					CDOActorClass = static_cast<TSubclassOf<AActor>>(RootBlueprintGeneratedClass);
					break;
				}
				TestOuter = TestOuter->GetOuter();
			}
		}

		if (!CDOActorClass && !Actor)
		{
			FilterHandle->MarkHiddenByCustomization();
			if (auto ComponentNameHandle = PropertyHandle->GetChildHandle(ComponentPicker::NameComponent, false))
			{
				HeaderRow
				.NameContent()
				[
					ComponentNameHandle->CreatePropertyNameWidget()
				]
				.ValueContent()
				[
					ComponentNameHandle->CreatePropertyValueWidget()
				];
			}
			return;
		}

		if (!Outer->HasAnyFlags(RF_ClassDefaultObject))
			FilterHandle->MarkHiddenByCustomization();

		Names.Reset();
		FName* MyValue = UnrealEditorUtils::GetPropertyMemberPtr<FName>(PropertyHandle, ComponentPicker::NameComponent);
		CurText = FText::FromName(*MyValue);
		if (*MyValue != NAME_None)
			Names.Add(MakeShared<FName>(NAME_None));
		auto SharedMyValue = MakeShared<FName>(*MyValue);
		Names.Add(SharedMyValue);
		for (auto& a : FComponentPickerCustomization::GetNames(Actor, CDOActorClass, FilterClass, SelfComponentName))
		{
			Names.Add(MakeShared<FName>(a));
		}
		TSharedPtr<SWidget> ValueWidget;
		HeaderRow
			.IsEnabled(!PropertyHandle->IsEditConst())
			.NameContent()
		[
				PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
			.MinDesiredWidth(260.f)
			.MaxDesiredWidth(0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SAssignNew(ValueWidget, SComboBox<TSharedPtr<FName>>)
				.OptionsSource(&Names)
				.InitiallySelectedItem(SharedMyValue)
				.OnGenerateWidget_Lambda([](TSharedPtr<FName> InName) {
					return SNew(STextBlock)
					.Text(FText::FromName(*InName));
				})
				.OnSelectionChanged_Lambda([this](TSharedPtr<FName> InName, ESelectInfo::Type SelectInfo) {
					if (SelectInfo == ESelectInfo::OnNavigation)
					{
						return;
					}
					auto ThisProperty = RootPropertyHandle.Pin();
					if (ThisProperty.IsValid())
					{
						if (auto CurName = UnrealEditorUtils::GetPropertyMemberPtr<FName>(ThisProperty, ComponentPicker::NameComponent))
						{
							CurText = FText::FromName(*InName);
							if (*InName != *CurName)
							{
								GEditor->BeginTransaction(TEXT("PropertyEditor"), NSLOCTEXT("LeveledPicker", "EditPropertyTransaction", "Edit LeveledPicker"), GetPropOwnerUObject(ThisProperty->GetProperty()));
								ThisProperty->NotifyPreChange();
								*CurName = *InName;
								ThisProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
								ThisProperty->NotifyFinishedChangingProperties();
								GEditor->EndTransaction();
							}
						}
					}
				})
				.Content()
				[
					SNew(STextBlock)
					.Text_Lambda([this] {
						auto ThisProperty = RootPropertyHandle.Pin();
						if (ThisProperty.IsValid())
						{
							if (FName* MyValue = UnrealEditorUtils::GetPropertyMemberPtr<FName>(ThisProperty, ComponentPicker::NameComponent))
							{
								return FText::FromName(*MyValue);
							}
						}
						return FText::FromName(NAME_None);
					})
				]
			]
		];
		ValueWidget->SetEnabled(!PropertyHandle->IsEditConst());
	}
}

void FComponentPickerCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	PropertyHandle->MarkHiddenByCustomization();
	// 	if (!CDOActorClass.Get())
	// 		return;
	//
	// 	TArray<UObject*> OuterObjects;
	// 	PropertyHandle->GetOuterObjects(OuterObjects);
	// 	if (OuterObjects.Num() == 1)
	// 	{
	// 		if (auto ComponentNameHandle = PropertyHandle->GetChildHandle(ComponentPicker::NameComponent, false))
	// 		{
	// 			ChildBuilder.AddProperty(ComponentNameHandle.ToSharedRef());
	// 			auto Utilities = CustomizationUtils.GetPropertyUtilities();
	// 			ComponentNameHandle->SetOnPropertyValueChanged(
	// 				FSimpleDelegate::CreateLambda([Utilities] { Utilities->ForceRefresh(); }));
	// 		}
	// 		// 		if (auto ComponentClassHandle = PropertyHandle->GetChildHandle(ComponentPicker::NameComponentClass, false))
	// 		// 		{
	// 		// 			ChildBuilder.AddProperty(ComponentClassHandle.ToSharedRef());
	// 		// 		}
	// 	}
}

TArray<FName> FComponentPickerCustomization::GetNames(AActor* Actor, TSubclassOf<AActor> InActorClass, TSubclassOf<UActorComponent> InClass, FName ExInclueName)
{
	TArray<FName> Ret;
	if (!InClass)
		InClass = UActorComponent::StaticClass();
	if (Actor)
	{
		for (auto& a : Actor->GetComponents())
		{
			if (a->IsA(InClass) && ExInclueName != a->GetFName())
				Ret.AddUnique(a->GetFName());
		}
	}
	else
	{
		// 	AActor* ActorCDO = InActorClass->GetDefaultObject<AActor>();
		// 	FObjectPropertyBase* ObjProp = FindFProperty<FObjectPropertyBase>(ActorCDO->GetClass(), CompnentName);
		// 	if (ObjProp)
		// 	{
		// 		if (auto Comp = Cast<UActorComponent>(ObjProp->GetObjectPropertyValue_InContainer(ActorCDO)))
		// 			Ret.AddUnique(Comp->GetFName());
		// 	}

		// Check blueprint nodes. Components added in blueprint editor only (and not in code) are not available from
		// CDO.
		UBlueprintGeneratedClass* RootBlueprintGeneratedClass = Cast<UBlueprintGeneratedClass>(InActorClass);
		UClass* ActorClass = InActorClass;

		// Go down the inheritance tree to find nodes that were added to parent blueprints of our blueprint graph.
		do
		{
			if (!IsValid(ActorClass))
			{
				return Ret;
			}

			UBlueprintGeneratedClass* ActorBlueprintGeneratedClass = Cast<UBlueprintGeneratedClass>(ActorClass);
			if (!ActorBlueprintGeneratedClass)
			{
				break;
			}

			const TArray<USCS_Node*>& ActorBlueprintNodes = ActorBlueprintGeneratedClass->SimpleConstructionScript->GetAllNodes();

			for (USCS_Node* Node : ActorBlueprintNodes)
			{
				if (Node->ComponentClass->IsChildOf(InClass))
				{
					if (ExInclueName != Node->GetVariableName())
						Ret.AddUnique(Node->GetVariableName());
				}
			}
			ActorClass = Cast<UClass>(ActorClass->GetSuperStruct());
		} while (ActorClass != AActor::StaticClass());
	}
	return Ret;
}
#endif
