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
