// Copyright GenericStorages, Inc. All Rights Reserved.

#include "GenericSystemConfigCustomization.h"

#if WITH_EDITOR
#include "Slate.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Editor.h"
#include "Editor/UnrealEditorUtils.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Level.h"
#include "Engine/Selection.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GenericSystems.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyTypeCustomization.h"
#include "IPropertyUtilities.h"
#include "Internationalization/Text.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "SNameComboBox.h"
#include "UObject/LinkerLoad.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UnrealType.h"

TSharedRef<IPropertyTypeCustomization> FGenericSystemConfigCustomization::MakeInstance()
{
	return MakeShareable(new FGenericSystemConfigCustomization);
}

void FGenericSystemConfigCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);
	if (OuterObjects.Num() == 1)
	{
		RootPropertyHandle = PropertyHandle;
		PropertyHandle->MarkResetToDefaultCustomized();
		auto ClassHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FGenericSystemConfig, SystemClass), false);
		check(ClassHandle.IsValid());

		SystemClass = UnrealEditorUtils::GetPropertyAddress<FGenericSystemConfig>(PropertyHandle)->SystemClass;
		HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(160.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SClassPropertyEntryBox)
				.MetaClass(AGenericSystemBase::StaticClass())
				.AllowAbstract(false)
				.AllowNone(false)
				.SelectedClass(SystemClass)
				.OnSetClass_Lambda([this](const UClass* Class) {
					auto ThisProperty = RootPropertyHandle.Pin();
					if (ThisProperty.IsValid())
					{
						auto ClassHandle = ThisProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FGenericSystemConfig, SystemClass), false);
						if (ClassHandle.IsValid())
						{
							SystemClass = const_cast<UClass*>(Class);
							ClassHandle->SetValue(SystemClass);
						}
					}
				})
			]
			// Use button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				PropertyCustomizationHelpers::MakeBrowseButton(FSimpleDelegate::CreateSP(this, &FGenericSystemConfigCustomization::BrowseTo), NSLOCTEXT("GenericSystems", "SelectInstance", "Select Instance"))
			]

		];
	}
}

void FGenericSystemConfigCustomization::BrowseTo()
{
	auto ThisProperty = RootPropertyHandle.Pin();
	if (!ThisProperty.IsValid())
		return;

	FGenericSystemConfig* OrignalPtr = UnrealEditorUtils::GetPropertyAddress<FGenericSystemConfig>(ThisProperty);
	check(OrignalPtr);

	if (AActor* Actor = OrignalPtr->SystemIns.Get())
	{
		if (USelection* Selection = GEditor->GetSelectedActors())
		{
			if (Selection->Num() == 1 && Selection->GetSelectedObject(0) == Actor)
			{
				GEditor->MoveViewportCamerasToActor(*Actor, false);
			}
			else
			{
				Selection->BeginBatchSelectOperation();
				GEditor->SelectNone(false, true, false);
				GEditor->SelectActor(Actor, true, true);
				Selection->EndBatchSelectOperation();
			}
		}
		else
		{
			GEditor->SelectActor(Actor, true, true);
		}
	}
}

#endif
