// Copyright GenericAbilities, Inc. All Rights Reserved.
#if WITH_EDITOR
#include "TypeTableBinddingCustomization.h"

#include "Slate.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Editor.h"
#include "Editor/UnrealEditorUtils.h"
#include "Engine/SimpleConstructionScript.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GameFramework/Actor.h"
#include "GenericSingletons.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyTypeCustomization.h"
#include "IPropertyUtilities.h"
#include "Misc/AssertionMacros.h"
#include "Modules/ModuleManager.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "TypeTableBindding.h"
#include "UObject/UnrealType.h"

TSharedRef<IPropertyTypeCustomization> FTypeToTableCustomization::MakeInstance()
{
	return MakeShareable(new FTypeToTableCustomization);
}

void FTypeToTableCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);
	if (OuterObjects.Num() == 1)
	{
		auto OuterObject = OuterObjects[0];
		RootPropertyHandle = PropertyHandle;
		auto WeakProperyHandle = RootPropertyHandle;

		auto OrignalPtr = UnrealEditorUtils::GetPropertyAddress<FTypeTableConfig>(PropertyHandle, OuterObject);
		check(OrignalPtr);
		ScriptStruct = FTypeTableBindding::FindType(OrignalPtr->Type);

		TWeakPtr<IPropertyUtilities> Utilities = CustomizationUtils.GetPropertyUtilities();
		FWeakObjectPtr WeakObject = OuterObject;

		TWeakPtr<FTypeToTableCustomization> WeakSelf = StaticCastSharedRef<FTypeToTableCustomization>(AsShared());
		auto Cb = FSimpleDelegate::CreateLambda([Utilities, WeakProperyHandle, WeakObject, WeakSelf] {
			auto Prop = WeakProperyHandle.Pin();
			auto This = WeakSelf.Pin();
			if (!Prop || !WeakObject.IsValid() || !This.IsValid())
				return;

			auto OrignalPtr = UnrealEditorUtils::GetPropertyAddress<FTypeTableConfig>(Prop, WeakObject.Get());
			auto FoundScriptStruct = FTypeTableBindding::FindType(OrignalPtr->Type);
			if (FoundScriptStruct && OrignalPtr->Table && (!FoundScriptStruct->IsChildOf(OrignalPtr->Table->RowStruct)))
			{
				if (auto TablePropertyHandle = Prop->GetChildHandle("Table", false))
				{
					TablePropertyHandle->SetValue((UObject*)nullptr, EPropertyValueSetFlags::NotTransactable);
					UnrealEditorUtils::ShowNotification(TEXT("Table Type MisMatch"));
				}
			}
			This->StructTypePicker.ScriptStruct = FoundScriptStruct;

			if (auto Util = Utilities.Pin())
				Util->RequestRefresh();
		});

		Cb.Execute();
		auto TypePropertyHandle = PropertyHandle->GetChildHandle("Type", false);
		TypePropertyHandle->SetOnPropertyValueChanged(Cb);

		auto TablePropertyHandle = PropertyHandle->GetChildHandle("Table", false);

		InitValue = MakeShared<FName>(NAME_None);
		{
			Names.Reset();
			Names.Add(InitValue);
			TArray<FString> Classes;
			FTypeTableBindding::Get().Binddings.GetKeys(Classes);
			for (auto& a : Classes)
			{
				if (auto Class = DynamicClass(a))
				{
					Names.Add(MakeShareable(new FName(*a)));
					if (OrignalPtr->Type && OrignalPtr->Type->GetName() == a)
						InitValue = Names.Last();
				}
			}
		}

		HeaderRow
		.NameContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(120)
			[
				SNew(SComboBox<TSharedPtr<FName>>)
				.OptionsSource(&Names)
				.InitiallySelectedItem(InitValue)
				.OnGenerateWidget_Lambda([](TSharedPtr<FName> InName) {
					return SNew(STextBlock)
					.Text(FText::FromName(*InName));
				})
				.OnSelectionChanged_Lambda([this, Utilities](TSharedPtr<FName> InName, ESelectInfo::Type SelectInfo) {
					if (SelectInfo == ESelectInfo::OnNavigation)
					{
						return;
					}
					auto Class = DynamicClass(InName->ToString());
					if (!Class)
						return;

					if (auto ThisProperty = RootPropertyHandle.Pin())
					{
						auto CurType = UnrealEditorUtils::GetPropertyMemberPtr<UClass*>(ThisProperty, TEXT("Type"));
						if (CurType)
						{
							auto& CurClass = *CurType;
							if (!CurClass || *InName != CurClass->GetFName())
							{
								GEditor->BeginTransaction(TEXT("PropertyEditor"), NSLOCTEXT("ExecutionConfig", "EditPropertyTransaction", "Edit ExecutionConfig"), GetPropertyOwnerUObject(ThisProperty->GetProperty()));
								InitValue = InName;
								CurClass = Class;
								ThisProperty->NotifyPreChange();
								ThisProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
								ThisProperty->NotifyFinishedChangingProperties();
								GEditor->EndTransaction();
							}
						}
						StructTypePicker.ScriptStruct = FTypeTableBindding::FindType(Class);

						if (auto Util = Utilities.Pin())
							Util->RequestRefresh();
					}
				})
				.Content()
				[
					SNew(STextBlock)
					.Text_Lambda([this] { return FText::FromName(*InitValue); })
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				PropertyCustomizationHelpers::MakeBrowseButton(FSimpleDelegate::CreateLambda([InitVal{this->InitValue}] {
					if (InitVal && !InitVal->IsNone())
					{
						auto Class = DynamicClass(InitVal->ToString());
						if (GEditor && Class)
						{
							TArray<UObject*> Objects;
							Objects.Add(Class);
							GEditor->SyncBrowserToObjects(Objects);
						}
					}
				}))
			]
		]
		.ValueContent()
		.MinDesiredWidth(250.f)
		.MaxDesiredWidth(0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(120)
			[
				StructTypePicker.MakeDynamicPropertyWidget(TablePropertyHandle, false)
			]
		];
	}
}
#endif
