// Copyright 2018-2020 wangjieest, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

#if WITH_EDITOR
#	include "Misc/AssertionMacros.h"
#	include "AssetData.h"
#	include "Widgets/DeclarativeSyntaxSupport.h"
#	include "GameFramework/Actor.h"
#	include "Modules/ModuleManager.h"
#	include "Internationalization/Text.h"
#	include "UObject/WeakObjectPtrTemplates.h"
#	include "DetailCategoryBuilder.h"
#	include "Slate.h"
#	include "ScopedTransaction.h"
#	include "PropertyCustomizationHelpers.h"
#	include "PropertyHandle.h"
#	include "DetailLayoutBuilder.h"
#	include "Editor.h"
#	include "IDetailChildrenBuilder.h"
#	include "IPropertyTypeCustomization.h"
#	include "IPropertyUtilities.h"
#	include "PropertyEditorModule.h"
#	include "Components/ActorComponent.h"
#	include "DataTableEditorUtils.h"
#	include "IDetailCustomization.h"
#	include "Templates/SubclassOf.h"
#	include "GS_Class2Name.h"

class AActor;
class UObject;
class UDataTable;

#	if ENGINE_MINOR_VERSION >= 22
using EPropertyClass = UE4CodeGen_Private::EPropertyGenFlags;
#	else
using EPropertyClass = UE4CodeGen_Private::EPropertyClass;
#	endif

namespace UnrealEditorUtils
{
template<typename T>
FORCEINLINE FName GetPropertyName()
{
	return GS_CLASS_TO_NAME::TClass2Name<T>::GetName();
}
GENERICSTORAGES_API FName GetPropertyName(FProperty* Property);
GENERICSTORAGES_API FName GetPropertyName(FProperty* Property, EPropertyClass PropertyEnum, EPropertyClass ValueEnum = (EPropertyClass)-1, EPropertyClass KeyEnum = (EPropertyClass)-1);

template<typename T>
T* GetPropertyHandleUObjectValue(TSharedPtr<IPropertyHandle> PropertyHandle)
{
	T* Ret = nullptr;
	UObject* Obj = nullptr;
	if (FPropertyAccess::Success == PropertyHandle->GetValue(Obj))
		Ret = Cast<T>(Obj);
	return Ret;
}

GENERICSTORAGES_API void* AccessPropertyAddress(const TSharedPtr<IPropertyHandle>& PropertyHandle);

template<typename ValueType>
ValueType* AccessPropertyAddress(const TSharedPtr<IPropertyHandle>& PropertyHandle, bool bEnsure = true)
{
	FProperty* Property = PropertyHandle.IsValid() ? PropertyHandle->GetProperty() : nullptr;
	if (Property && GetPropertyName<ValueType>() == GetPropertyName(Property))
		return static_cast<ValueType*>(AccessPropertyAddress(PropertyHandle));
	ensureAlways(!bEnsure);
	return nullptr;
}

// TArray/TSet/TMap/Element
GENERICSTORAGES_API void* GetPropertyAddress(const TSharedPtr<IPropertyHandle>& PropertyHandle, UObject* Outer = nullptr, void** ContainerAddr = nullptr);
template<typename StructType>
StructType* GetPropertyAddress(const TSharedPtr<IPropertyHandle>& PropertyHandle, UObject* Outer = nullptr, void** ContainerAddr = nullptr, bool bEnsure = true)
{
	FStructProperty* StructProperty = PropertyHandle.IsValid() ? CastField<FStructProperty>(PropertyHandle->GetProperty()) : nullptr;
	if (StructProperty && GetPropertyName<StructType>() == GetPropertyName(StructProperty))
		return static_cast<StructType*>(GetPropertyAddress(PropertyHandle, Outer, ContainerAddr));
	ensureAlways(!bEnsure);
	return nullptr;
}

// Get Property Member In Struct
GENERICSTORAGES_API void* GetPropertyMemberPtr(const TSharedPtr<IPropertyHandle>& PropertyHandle, FName MemberName);

template<typename ValueType>
ValueType* GetPropertyMemberPtr(const TSharedPtr<IPropertyHandle>& PropertyHandle, FName MemberName, bool bEnsure = true)
{
	FStructProperty* StructProperty = PropertyHandle.IsValid() ? CastField<FStructProperty>(PropertyHandle->GetProperty()) : nullptr;
	FProperty* MemberProerty = StructProperty ? StructProperty->Struct->FindPropertyByName(MemberName) : nullptr;
	if (MemberProerty && GetPropertyName<ValueType>() == GetPropertyName(MemberProerty))
		return static_cast<ValueType*>(GetPropertyMemberPtr(PropertyHandle, MemberName));
	ensureAlways(!bEnsure);
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////

struct GENERICSTORAGES_API FDatatableTypePicker
{
public:
	FDatatableTypePicker(TSharedPtr<IPropertyHandle> RootHandle = nullptr);

	TSharedPtr<SWidget> MyWidget;
	TBaseDelegate<void> OnChanged;
	TArray<UScriptStruct*> RowStructs;
	TWeakObjectPtr<UScriptStruct> ScriptStruct;
	FString FilterPath;
	bool bValidate = false;

	void Init(TSharedPtr<IPropertyHandle> RootHandle);

	TSharedRef<SWidget> MakeWidget();
	TSharedRef<SWidget> MakePropertyWidget(TSharedPtr<IPropertyHandle> SoftHandle, bool bAllowClear = true);
	TSharedRef<SWidget> MakeDynamicPropertyWidget(TSharedPtr<IPropertyHandle> SoftHandle, bool bAllowClear = true);

	static TWeakObjectPtr<UScriptStruct> FindTableType(UDataTable* Table);
	static TWeakObjectPtr<UScriptStruct> FindTableType(TSoftObjectPtr<UDataTable> Table);
	static bool UpdateTableType(UDataTable* Table);
	static bool UpdateTableType(TSoftObjectPtr<UDataTable> Table);
};

//////////////////////////////////////////////////////////////////////////

class GENERICSTORAGES_API FScopedPropertyTransaction
{
public:
	FScopedPropertyTransaction(const TSharedPtr<IPropertyHandle>& InPropertyHandle,
							   const FText& SessionName = NSLOCTEXT("UnrealEditorUtils", "ScopedTransaction", "ScopedTransaction"),
							   const TCHAR* TransactionContext = TEXT("UnrealEditorUtils"));
	~FScopedPropertyTransaction();

protected:
	IPropertyHandle* const PropertyHandle;
	FScopedTransaction Scoped;
};

GENERICSTORAGES_API void ShowNotification(const FString& Tip, bool bSucc = false);
GENERICSTORAGES_API bool UpdatePropertyViews(const TArray<UObject*>& Objects);

GENERICSTORAGES_API TArray<FSoftObjectPath> EditorSyncScan(UClass* Type, const FString& Dir = TEXT("/Game"));
GENERICSTORAGES_API bool EditorAsyncLoad(UClass* Type, const FString& Dir = TEXT("/Game"), bool bOnce = true);

GENERICSTORAGES_API bool PinTypeFromString(FString TypeString, FEdGraphPinType& OutPinType, bool bInTemplate = false, bool bInContainer = false);
GENERICSTORAGES_API FString GetDefaultValueOnType(const FEdGraphPinType& PinType);
}  // namespace UnrealEditorUtils

#endif
