// Copyright 2018-2020 wangjieest, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#	include "EdGraphUtilities.h"
#	include "Input/Reply.h"
#	include "KismetPins/SGraphPinObject.h"
#	include "SGraphPin.h"
#	include "Widgets/DeclarativeSyntaxSupport.h"
#	include "Widgets/SWidget.h"

// CustomClassPinPicker with MetaClass [AllowedClasses AllowAbstract MustImplement]
class CustomClassPinPicker;
class SClassPickerGraphPin : public SGraphPinObject
{
public:
	SLATE_BEGIN_ARGS(SClassPickerGraphPin)
		{
		}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

	static bool IsMatchedPinType(UEdGraphPin* InGraphPinObj);
	static bool IsMatchedToCreate(UEdGraphPin* InGraphPinObj);

protected:
	static bool IsCustomClassPinPicker(UEdGraphPin* InGraphPinObj);
	static bool GetMetaClassData(UEdGraphPin* InGraphPinObj, const UClass*& ImplementClass, TSet<const UClass*>& AllowClasses);

	// Called when a new class was picked via the asset picker
	void OnPickedNewClass(UClass* ChosenClass);

	//~ Begin SGraphPinObject Interface
	virtual FReply OnClickUse() override;
	virtual bool AllowSelfPinWidget() const override { return false; }
	virtual TSharedRef<SWidget> GenerateAssetPicker() override;
	virtual FText GetDefaultComboText() const override;
	virtual FOnClicked GetOnUseButtonDelegate() override;
	virtual const FAssetData& GetAssetData(bool bRuntimePath) const override;
	//~ End SGraphPinObject Interface

	/** Cached AssetData without the _C */
	mutable FAssetData CachedEditorAssetData;
};

// CustomObjectFilter with [MetaClass]
class CustomObjectFilter;
class SObjectFilterGraphPin : public SGraphPinObject
{
public:
	SLATE_BEGIN_ARGS(SObjectFilterGraphPin)
		{
		}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);

	static bool IsMatchedPinType(UEdGraphPin* InGraphPinObj);
	static bool IsMatchedToCreate(UEdGraphPin* InGraphPinObj);

protected:
	static bool IsCustomObjectFilter(UEdGraphPin* InGraphPinObj);
	static bool SetMetaClassData(UEdGraphPin* InGraphPinObj);

	//~ Begin SGraphPinObject Interface
	virtual bool AllowSelfPinWidget() const override { return false; }
	//~ End SGraphPinObject Interface
};
#endif
