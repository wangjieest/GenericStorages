// Copyright GenericStorages, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR
#include "CoreMinimal.h"

#include "Input/Reply.h"
#include "KismetPins/SGraphPinObject.h"
#include "SGraphPin.h"
#include "UnrealCompatibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"

class GENERICSTORAGES_API SGraphPinObjectExtra : public SGraphPinObject
{
public:
	SLATE_BEGIN_ARGS(SGraphPinObjectExtra) {}
	SLATE_END_ARGS()

protected:
	virtual bool AllowSelfPinWidget() const override { return false; }
	static bool HasCustomMeta(UEdGraphPin* InGraphPinObj, const TCHAR* MetaName, TSet<FString>* Out = nullptr);
	virtual FReply OnClickUse() override;
	virtual bool OnCanUseAssetData(const FAssetData& AssetData);
	virtual EVisibility GetDefaultValueVisibility() const;
	bool bHiddenDefaultValue = false;
};

// CustomClassPinPicker with MetaClass [AllowedClasses DisallowedClasses AllowAbstract MustImplement NotConnectable WithinMetaKey WithoutMetaKey]
class CustomClassPinPicker;
class GENERICSTORAGES_API SClassPickerGraphPin : public SGraphPinObjectExtra
{
public:
	SLATE_BEGIN_ARGS(SClassPickerGraphPin) {}
	SLATE_ARGUMENT_DEFAULT(bool, bRequiredMatch){true};
	SLATE_ARGUMENT_DEFAULT(bool, bOnlyClass){true};
	SLATE_ARGUMENT_DEFAULT(bool, bNotConnectable){false};
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj, FProperty* InBindProp = nullptr);

	static bool IsMatchedPinType(UEdGraphPin* InGraphPinObj, bool bOnlyCls = true);
	static bool IsMatchedToCreate(UEdGraphPin* InGraphPinObj, bool bOnlyCls = true);
	static UClass* GetChoosenClass(UEdGraphPin* InGraphPinObj);
	static UStruct* GetChoosenStruct(UEdGraphPin* InGraphPinObj, bool bOnlyCls = true);

protected:
	friend struct FTypePickerGraphPinUtils;
	bool SetMetaInfo(UEdGraphPin* InGraphPinObj);

	void OnPickedNewClass(UClass* ChosenClass);

	void OnPickedNewStruct(const UScriptStruct* ChosenStruct);

	//~ Begin SGraphPinObject Interface
	virtual FReply OnClickUse() override;
	virtual TSharedRef<SWidget> GenerateAssetPicker() override;
	virtual FText GetDefaultComboText() const override;
	virtual const FAssetData& GetAssetData(bool bRuntimePath) const override;
	//~ End SGraphPinObject Interface

	/** Cached AssetData without the _C */
	mutable FAssetData CachedEditorAssetData;
	FProperty* BindProp = nullptr;
	TOptional<bool> bNotConnectable;
	TSharedPtr<class IClassViewerFilter> ClassFilter;
	TSharedPtr<class IStructViewerFilter> StructFilter;
	bool bRequiredMatch = true;
	bool bOnlyClass = true;
};

// CustomObjectPinPicker with [MetaClass, NotConnectable RequrieConnection]
class CustomObjectPinPicker;
class GENERICSTORAGES_API SObjectFilterGraphPin : public SGraphPinObjectExtra
{
public:
	SLATE_BEGIN_ARGS(SObjectFilterGraphPin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

	static bool IsMatchedPinType(UEdGraphPin* InGraphPinObj);
	static bool IsMatchedToCreate(UEdGraphPin* InGraphPinObj);

protected:
	virtual const FAssetData& GetAssetData(bool bRuntimePath) const override;
	mutable FAssetData CachedEditorAssetData;
	TSharedPtr<class IClassViewerFilter> ClassFilter;

	virtual TSharedRef<SWidget> GenerateAssetPicker() override;
	bool SetMetaInfo(UEdGraphPin* InGraphPinObj);
	TWeakObjectPtr<UClass> WeakMetaClass;
};

// CustomDatatableFilter with [DataTableMetaStruct DisableNoneSelection NotConnectable RequrieConnection]
class CustomDataTableFilter;
class GENERICSTORAGES_API SDataTableFilterGraphPin : public SGraphPinObjectExtra
{
public:
	SLATE_BEGIN_ARGS(SDataTableFilterGraphPin) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

	static bool IsMatchedPinType(UEdGraphPin* InGraphPinObj);
	static bool IsMatchedToCreate(UEdGraphPin* InGraphPinObj);

protected:
	virtual bool OnCanUseAssetData(const FAssetData& AssetData) override;

	bool SetMetaInfo(UEdGraphPin* InGraphPinObj);
	virtual TSharedRef<SWidget> GenerateAssetPicker() override;
	TSet<FString> StructNames;
	bool bDisableNoneSelection = false;
	bool bRequrieConnection = false;
};

namespace CustomGraphPinPicker
{
void RegInterfaceClassSelectorFactory();
void RegInterfaceObjectFilterFactory();
void RegInterfaceDataTableFilterFactory();
}  // namespace CustomGraphPinPicker
#endif
