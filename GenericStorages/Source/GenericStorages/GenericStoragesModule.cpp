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

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#	include "PropertyEditorDelegates.h"
#	include "PropertyEditorModule.h"
#endif

#include "DataTablePickerCustomization.h"
#include "SClassPickerGraphPin.h"

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
			PropertyModule->NotifyCustomizationModuleChanged();
#endif
		}
	}

	virtual void ShutdownModule() override {}
	// End of IModuleInterface implementation
};

IMPLEMENT_MODULE(FGenericStoragesPlugin, GenericStorages)
