// Copyright GenericStorages, Inc. All Rights Reserved.

#pragma once
#if WITH_EDITOR
#include "CoreMinimal.h"

#include "Components/ActorComponent.h"
#include "DataTableEditorUtils.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Editor.h"
#include "GameFramework/Actor.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailCustomization.h"
#include "IPropertyTypeCustomization.h"
#include "IPropertyUtilities.h"
#include "Internationalization/Text.h"
#include "Misc/AssertionMacros.h"
#include "Modules/ModuleManager.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Templates/SubclassOf.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "UnrealCompatibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "EdGraph/EdGraphPin.h"
#include "AssetRegistry/AssetRegistryModule.h"
#if defined(GMP_API)
#include "GMPCore.h"
#endif

class AActor;
class UObject;
class UDataTable;
class ISettingsModule;
class SWidget;

namespace UnrealEditorUtils
{
template<typename T, bool bExactType = true>
bool VerifyPropName(FProperty* Property)
{
#if defined(GMP_API)
	return Property && GMPReflection::EqualPropertyType<T, bExactType>(Property);
#else
	return !!Property;
#endif
}

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
	if (!VerifyPropName<ValueType>(Property))
	{
		ensureAlways(!bEnsure);
		return nullptr;
	}
	return static_cast<ValueType*>(AccessPropertyAddress(PropertyHandle));
}

// TArray/TSet/TMap/Element
GENERICSTORAGES_API void* GetPropertyAddress(const TSharedPtr<IPropertyHandle>& PropertyHandle, UObject* Outer = nullptr, void** ContainerAddr = nullptr);
template<typename StructType>
StructType* GetPropertyAddress(const TSharedPtr<IPropertyHandle>& PropertyHandle, UObject* Outer = nullptr, void** ContainerAddr = nullptr, bool bEnsure = true)
{
	FStructProperty* StructProperty = PropertyHandle.IsValid() ? CastField<FStructProperty>(PropertyHandle->GetProperty()) : nullptr;
	if (!VerifyPropName<StructType>(StructProperty))
	{
		ensureAlways(!bEnsure);
		return nullptr;
	}
	return static_cast<StructType*>(GetPropertyAddress(PropertyHandle, Outer, ContainerAddr));
}

// Get Property Member In Struct
GENERICSTORAGES_API void* GetPropertyMemberPtr(const TSharedPtr<IPropertyHandle>& PropertyHandle, FName MemberName);

template<typename ValueType>
ValueType* GetPropertyMemberPtr(const TSharedPtr<IPropertyHandle>& PropertyHandle, FName MemberName, bool bEnsure = true)
{
	FStructProperty* StructProperty = PropertyHandle.IsValid() ? CastField<FStructProperty>(PropertyHandle->GetProperty()) : nullptr;
	FProperty* MemberProerty = StructProperty ? FindFProperty<FProperty>(StructProperty->Struct, MemberName) : nullptr;
	if (!VerifyPropName<ValueType>(MemberProerty))
	{
		ensureAlways(!bEnsure);
		return nullptr;
	}
	return static_cast<ValueType*>(GetPropertyMemberPtr(PropertyHandle, MemberName));
}

//////////////////////////////////////////////////////////////////////////

struct GENERICSTORAGES_API FDatatableTypePicker
{
public:
	FDatatableTypePicker(TSharedPtr<IPropertyHandle> RootHandle = nullptr);

	TSharedPtr<SWidget> MyWidget;
	TDelegate<void()> OnChanged;
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

GENERICSTORAGES_API void ShowNotification(const FText& Tip, bool bSucc = false, float FadeInDuration = 1.0f, float FadeOutDuration = 2.0f, float ExpireDuration = 3.0f);
inline void ShowNotification(const FString& Tip, bool bSucc = false, float FadeInDuration = 1.0f, float FadeOutDuration = 2.0f, float ExpireDuration = 3.0f)
{
	ShowNotification(FText::FromString(Tip), bSucc, FadeInDuration, FadeOutDuration, ExpireDuration);
}
GENERICSTORAGES_API bool UpdatePropertyViews(const TArray<UObject*>& Objects);
GENERICSTORAGES_API void ReplaceViewedObjects(const TMap<UObject*, UObject*>& OldToNewObjectMap);
GENERICSTORAGES_API void RemoveDeletedObjects(TArray<UObject*> DeletedObjects);

GENERICSTORAGES_API TArray<FSoftObjectPath> EditorSyncScan(UClass* Type, const FString& Dir = TEXT("/Game"));
GENERICSTORAGES_API bool EditorAsyncLoad(UClass* Type, const FString& Dir = TEXT("/Game"), bool bOnce = true);

GENERICSTORAGES_API bool IdentifyPinValueOnProperty(UEdGraphPin* GraphPinObj, UObject* Obj, FProperty* Prop, bool bReset = false);

GENERICSTORAGES_API bool ShouldUseStructReference(UEdGraphPin* TestPin);

GENERICSTORAGES_API ISettingsModule* GetSettingsModule();
GENERICSTORAGES_API bool RegisterSettings(const FName& ContainerName,
										  const FName& CategoryName,
										  const FName& SectionName,
										  const FText& DisplayText,
										  const FText& DescriptionText,
										  const TWeakObjectPtr<UObject>& SettingsObject,
										  ISettingsModule* InSettingsModule = nullptr);

struct FConfigurationPos
{
	FName SectionName;
	FName CategoryName;
	FName ContainerName;

	FConfigurationPos(FName InSectionName = NAME_None, FName InCategoryName = "ExtraConfiguration", FName InContainerName = "Project")
		: SectionName(InSectionName)
		, CategoryName(InCategoryName)
		, ContainerName(InContainerName)
	{
	}
};

GENERICSTORAGES_API bool AddConfigurationOnProject(UObject* Obj, const FConfigurationPos& NameCfg = {}, bool bOnlyCDO = true, bool bOnlyNative = true, bool bAllowAbstract = false);
GENERICSTORAGES_API bool ShouldEndPlayMap();
}  // namespace UnrealEditorUtils
#else
namespace UnrealEditorUtils
{
struct FConfigurationPos
{
	FName SectionName;
	FName CategoryName;
	FName ContainerName;

	FConfigurationPos(FName InSectionName = NAME_None, FName InCategoryName = "ExtraConfiguration", FName InContainerName = "Project")
		: SectionName(InSectionName)
		, CategoryName(InCategoryName)
		, ContainerName(InContainerName)
	{
	}
};

template<typename T>
GENERICSTORAGES_API bool AddConfigurationOnProject(UObject* Obj, const T&, bool bOnlyCDO = true, bool bOnlyNative = true, bool bAllowAbstract = false)
{
	return false;
}
FORCEINLINE bool ShouldEndPlayMap()
{
	return false;
}
}  // namespace UnrealEditorUtils
#endif
