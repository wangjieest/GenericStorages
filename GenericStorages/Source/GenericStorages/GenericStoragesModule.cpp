// Copyright 2018-2020 wangjieest, Inc. All Rights Reserved.

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#	include "PropertyEditorDelegates.h"
#	include "PropertyEditorModule.h"
#	include "Editor/DataTablePickerCustomization.h"
#	include "Editor/SClassPickerGraphPin.h"
#	include "Editor/ComponentPickerCustomization.h"
#endif

class FGenericStoragesPlugin final : public IModuleInterface
{
public:
	// IModuleInterface implementation
	virtual void StartupModule() override
	{
#if WITH_EDITOR
		class FInterfaceClassSelectorPinFactory : public FGraphPanelPinFactory
		{
			virtual TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* InPin) const override
			{
				if (SClassPickerGraphPin::IsMatchedToCreate(InPin))
				{
					return SNew(SClassPickerGraphPin, InPin);
				}
				return nullptr;
			}
		};
		FEdGraphUtilities::RegisterVisualPinFactory(MakeShareable(new FInterfaceClassSelectorPinFactory()));

		class FInterfaceObjectFilterPinFactory : public FGraphPanelPinFactory
		{
			virtual TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* InPin) const override
			{
				if (SObjectFilterGraphPin::IsMatchedToCreate(InPin))
				{
					return SNew(SObjectFilterGraphPin, InPin);
				}
				return nullptr;
			}
		};
		FEdGraphUtilities::RegisterVisualPinFactory(MakeShareable(new FInterfaceObjectFilterPinFactory()));

		auto PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor");
		if (PropertyModule)
		{
			PropertyModule->RegisterCustomPropertyTypeLayout("DataTablePicker", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDataTablePickerCustomization::MakeInstance));
			PropertyModule->RegisterCustomPropertyTypeLayout("DataTablePathPicker", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDataTablePathPickerCustomization::MakeInstance));
			PropertyModule->RegisterCustomPropertyTypeLayout("DataTableRowNamePicker", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDataTableRowNamePickerCustomization::MakeInstance));
			PropertyModule->RegisterCustomPropertyTypeLayout("DataTableRowPicker", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDataTableRowPickerCustomization::MakeInstance));
			PropertyModule->RegisterCustomPropertyTypeLayout("ComponentPicker", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FComponentPickerCustomization::MakeInstance));
			PropertyModule->NotifyCustomizationModuleChanged();
		}
#endif
	}

	virtual void ShutdownModule() override {}
	// End of IModuleInterface implementation
};

IMPLEMENT_MODULE(FGenericStoragesPlugin, GenericStorages)
