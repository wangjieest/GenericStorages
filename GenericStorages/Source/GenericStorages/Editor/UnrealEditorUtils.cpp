// Copyright 2018-2020 wangjieest, Inc. All Rights Reserved.

#include "UnrealEditorUtils.h"

#if WITH_EDITOR
#	include "ARFilter.h"
#	include "AssetRegistryModule.h"
#	include "DataTableEditorUtils.h"
#	include "EdGraph/EdGraphPin.h"
#	include "EdGraphSchema_K2.h"
#	include "Engine/AssetManager.h"
#	include "Engine/DataTable.h"
#	include "Engine/ObjectLibrary.h"
#	include "Engine/StreamableManager.h"
#	include "Engine/UserDefinedStruct.h"
#	include "Engine/World.h"
#	include "Framework/Notifications/NotificationManager.h"
#	include "GameFramework/Actor.h"
#	include "IAssetRegistry.h"
#	include "Misc/PackageName.h"
#	include "PropertyEditorModule.h"
#	include "UObject/ObjectKey.h"
#	include "UObject/TextProperty.h"
#	include "UObject/UObjectGlobals.h"
#	include "Widgets/Notifications/SNotificationList.h"
#	include "GenericSingletons.h"

namespace UnrealEditorUtils
{
FName GetPropertyName(FProperty* InProperty)
{
#	if 1
	using ValueType = FName (*)(FProperty * Property);
	using KeyType = const FFieldClass*;
#		define GS_DEF_PAIR_CELL(PropertyTypeName) TPairInitializer<const KeyType&, const ValueType&>(PropertyTypeName::StaticClass(), [](FProperty* Property) { return GetPropertyName<typename PropertyTypeName::TCppType>(); })
#		define GS_DEF_PAIR_CELL_CUSTOM(PropertyTypeName, ...) TPairInitializer<const KeyType&, const ValueType&>(PropertyTypeName::StaticClass(), [](FProperty* Property) { return __VA_ARGS__; })

	// clang-format off
	static TMap<KeyType, ValueType> DispatchMap {
		GS_DEF_PAIR_CELL_CUSTOM(FStructProperty, FName(*CastField<FStructProperty>(Property)->Struct->GetName())),
		GS_DEF_PAIR_CELL(FBoolProperty),
		GS_DEF_PAIR_CELL(FInt8Property),
		GS_DEF_PAIR_CELL(FByteProperty),
		GS_DEF_PAIR_CELL(FInt16Property),
		GS_DEF_PAIR_CELL(FUInt16Property),
		GS_DEF_PAIR_CELL(FIntProperty),
		GS_DEF_PAIR_CELL(FUInt32Property),
		GS_DEF_PAIR_CELL(FInt64Property),
		GS_DEF_PAIR_CELL(FUInt64Property),
		GS_DEF_PAIR_CELL(FFloatProperty),
		GS_DEF_PAIR_CELL(FDoubleProperty),
		GS_DEF_PAIR_CELL_CUSTOM(FEnumProperty, GetPropertyName(CastField<FEnumProperty>(Property)->GetUnderlyingProperty())),

		GS_DEF_PAIR_CELL(FStrProperty),
		GS_DEF_PAIR_CELL(FNameProperty),
		GS_DEF_PAIR_CELL(FTextProperty),

		GS_DEF_PAIR_CELL(FDelegateProperty),

		GS_DEF_PAIR_CELL(FLazyObjectProperty),
		GS_DEF_PAIR_CELL(FSoftObjectProperty),
		GS_DEF_PAIR_CELL(FWeakObjectProperty),
		GS_DEF_PAIR_CELL(FInterfaceProperty),
		GS_DEF_PAIR_CELL_CUSTOM(FObjectProperty, GetPropertyName<UObject>()),
		GS_DEF_PAIR_CELL_CUSTOM(FClassProperty, GetPropertyName<UObject>()),
		GS_DEF_PAIR_CELL_CUSTOM(FSoftClassProperty, GetPropertyName<FSoftObjectPtr>()),

		GS_DEF_PAIR_CELL_CUSTOM(FArrayProperty, GS_CLASS_TO_NAME::TTraitsTemplate<void>::GetTArrayName(*GetPropertyName(CastField<FArrayProperty>(Property)->Inner).ToString())),
		GS_DEF_PAIR_CELL_CUSTOM(FSetProperty, GS_CLASS_TO_NAME::TTraitsTemplate<void>::GetTSetName(*GetPropertyName(CastField<FSetProperty>(Property)->ElementProp).ToString())),
		GS_DEF_PAIR_CELL_CUSTOM(FMapProperty, GS_CLASS_TO_NAME::TTraitsTemplate<void>::GetTMapName(*GetPropertyName(CastField<FMapProperty>(Property)->KeyProp).ToString(), *GetPropertyName(CastField<FMapProperty>(Property)->ValueProp).ToString()))
	};
	// clang-format on

#		undef GS_DEF_PAIR_CELL
#		undef GS_DEF_PAIR_CELL_CUSTOM

	if (auto Func = DispatchMap.Find(InProperty->GetClass()))
	{
		return (*Func)(InProperty);
	}
#	else

#		define DispathPropertyType(PropertyTypeName) \
			else if (CastField<PropertyTypeName>(InProperty)) { return GetPropertyName<typename PropertyTypeName::TCppType>(); }

	// clang-format off
	if (FStructProperty* SubStructProperty = CastField<FStructProperty>(InProperty))
	{
		return *SubStructProperty->Struct->GetName();
	}
	DispathPropertyType(FBoolProperty)
	DispathPropertyType(FInt8Property)
	DispathPropertyType(FByteProperty)
	DispathPropertyType(FInt16Property)
	DispathPropertyType(FUInt16Property)
	DispathPropertyType(FIntProperty)
	DispathPropertyType(FUInt32Property)
	DispathPropertyType(FInt64Property)
	DispathPropertyType(FUInt64Property)
	DispathPropertyType(FFloatProperty)
	DispathPropertyType(FDoubleProperty)
	DispathPropertyType(FDelegateProperty)
	DispathPropertyType(FStrProperty)
	DispathPropertyType(FNameProperty)
	DispathPropertyType(FTextProperty)
	else if (FEnumProperty* SubEnumProperty = CastField<FEnumProperty>(InProperty))
	{
		return GetPropertyName(SubEnumProperty->GetUnderlyingProperty());
	}
	// clang-format on
	else if (CastField<FObjectPropertyBase>(InProperty))
	{
		// [PropertyClass] is the detail type of target
		// Weak
		if (CastField<FWeakObjectProperty>(InProperty))
		{
			// FWeakObjectPtr
			return GetPropertyName<FWeakObjectProperty::TCppType>();
		}
		// FClassProperty is a specialization of FObjectProperty with MetaClass
		// Object
		else if (CastField<FObjectProperty>(InProperty))
		{
			// UObject*
			return GetPropertyName<UObject>();
		}
		// FSoftClassProperty is a specialization of FSoftObjectProperty with MetaClass
		// SoftObject
		else if (CastField<FSoftObjectProperty>(InProperty))
		{
			// FSoftObjectPtr
			return GetPropertyName<FSoftObjectProperty::TCppType>();
		}
		// Lazy
		else if (CastField<FLazyObjectProperty>(InProperty))
		{
			// FLazyObjectPtr
			return GetPropertyName<FLazyObjectProperty::TCppType>();
		}
		else
		{
			// otherwise raw pointer
			return GetPropertyName<UObject>();
		}
	}
	else if (FArrayProperty* SubArrProperty = CastField<FArrayProperty>(InProperty)) { return GS_CLASS_TO_NAME::TTraitsTemplate<void>::GetTArrayName(*GetPropertyName(SubArrProperty->Inner).ToString()); }
	else if (FMapProperty* SubMapProperty = CastField<FMapProperty>(InProperty))
	{
		return GS_CLASS_TO_NAME::TTraitsTemplate<void>::GetTMapName(*GetPropertyName(SubMapProperty->KeyProp).ToString(), *GetPropertyName(SubMapProperty->ValueProp).ToString());
	}
	else if (FSetProperty* SubSetProperty = CastField<FSetProperty>(InProperty)) { return GS_CLASS_TO_NAME::TTraitsTemplate<void>::GetTSetName(*GetPropertyName(SubSetProperty->ElementProp).ToString()); }
	else if (FInterfaceProperty* SubIncProperty = CastField<FInterfaceProperty>(InProperty))
	{
		// FScriptInterface
		return GetPropertyName<FInterfaceProperty::TCppType>();
	}
#	endif
	else
	{
		ensureAlways(false);
		return NAME_None;
	}
	return NAME_None;
}

FName GetPropertyName(FProperty* InProperty, EPropertyClass PropertyEnum, EPropertyClass ValueEnum, EPropertyClass KeyEnum)
{
	using KeyType = EPropertyClass;
	using ValueType = FName (*)(FProperty * Property, EPropertyClass, EPropertyClass);
#	define GS_DEF_PAIR_CELL(Index) \
		TPairInitializer<const KeyType&, const ValueType&>(KeyType::Index, [](FProperty* Property, EPropertyClass InValueEnum, EPropertyClass InKeyEnum) { return GetPropertyName<typename F##Index##Property ::TCppType>(); })

#	define GS_DEF_PAIR_CELL_CUSTOM(Index, ...) TPairInitializer<const KeyType&, const ValueType&>(KeyType::Index, [](FProperty* Property, EPropertyClass InValueEnum, EPropertyClass InKeyEnum) { return __VA_ARGS__; })

	static TMap<KeyType, ValueType> DispatchMap{
		GS_DEF_PAIR_CELL_CUSTOM(Struct, FName(*CastField<FStructProperty>(Property)->Struct->GetName())),

		GS_DEF_PAIR_CELL(Bool),
		GS_DEF_PAIR_CELL(Int8),
		GS_DEF_PAIR_CELL(Byte),
		GS_DEF_PAIR_CELL(Int16),
		GS_DEF_PAIR_CELL(UInt16),
		GS_DEF_PAIR_CELL(Int),
		GS_DEF_PAIR_CELL(UInt32),
		GS_DEF_PAIR_CELL(UInt64),
		GS_DEF_PAIR_CELL(UnsizedInt),
		GS_DEF_PAIR_CELL(UnsizedUInt),
		GS_DEF_PAIR_CELL(Float),
		GS_DEF_PAIR_CELL(Double),
		GS_DEF_PAIR_CELL_CUSTOM(Enum, GetPropertyName(CastField<FEnumProperty>(Property)->GetUnderlyingProperty())),
		GS_DEF_PAIR_CELL(Str),
		GS_DEF_PAIR_CELL(Name),
		GS_DEF_PAIR_CELL(Text),

		GS_DEF_PAIR_CELL(Delegate),
		//GS_DEF_PAIR_CELL(InlineMulticastDelegate),

		GS_DEF_PAIR_CELL(SoftClass),
		GS_DEF_PAIR_CELL(WeakObject),
		GS_DEF_PAIR_CELL(LazyObject),
		GS_DEF_PAIR_CELL(SoftObject),
		GS_DEF_PAIR_CELL(Interface),
		GS_DEF_PAIR_CELL_CUSTOM(Object, GetPropertyName<UObject>()),
		GS_DEF_PAIR_CELL_CUSTOM(Class, GetPropertyName<UObject>()),

		GS_DEF_PAIR_CELL_CUSTOM(Array, GS_CLASS_TO_NAME::TTraitsTemplate<void>::GetTArrayName(*GetPropertyName(CastField<FArrayProperty>(Property)->Inner, InValueEnum).ToString())),
		GS_DEF_PAIR_CELL_CUSTOM(Map, GS_CLASS_TO_NAME::TTraitsTemplate<void>::GetTMapName(*GetPropertyName(CastField<FMapProperty>(Property)->KeyProp, InKeyEnum).ToString(), *GetPropertyName(CastField<FMapProperty>(Property)->ValueProp, InValueEnum).ToString())),
		GS_DEF_PAIR_CELL_CUSTOM(Set, GS_CLASS_TO_NAME::TTraitsTemplate<void>::GetTSetName(*GetPropertyName(CastField<FSetProperty>(Property)->ElementProp, InValueEnum).ToString())),
	};

#	undef GS_DEF_PAIR_CELL
#	undef GS_DEF_PAIR_CELL_CUSTOM

	if (auto Func = DispatchMap.Find(PropertyEnum))
	{
		return (*Func)(InProperty, ValueEnum, KeyEnum);
	}
	else
	{
		return GetPropertyName(InProperty);
	}
}

//////////////////////////////////////////////////////////////////////////
bool UpdatePropertyViews(const TArray<UObject*>& Objects)
{
	if (FPropertyEditorModule* Module = FModuleManager::LoadModulePtr<FPropertyEditorModule>(TEXT("PropertyEditor")))
	{
		Module->UpdatePropertyViews(Objects);
		return true;
	}
	return false;
}

TArray<FSoftObjectPath> EditorSyncScan(UClass* Type, const FString& Dir)
{
	if (GIsEditor)
	{
		TArray<FSoftObjectPath> SoftPaths;
		if (Type && !IsRunningCommandlet())
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(FName("AssetRegistry"));
			IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
			TArray<FString> ContentPaths;
			ContentPaths.Add(Dir);
			AssetRegistry.ScanPathsSynchronous(ContentPaths);
			FARFilter Filter;
			Filter.ClassNames.Add(Type->GetFName());
			Filter.bRecursiveClasses = true;
			Filter.PackagePaths.Add(*Dir);
			Filter.bRecursivePaths = true;
			TArray<FAssetData> AssetList;
			AssetRegistry.GetAssets(Filter, AssetList);
			for (auto& Asset : AssetList)
				SoftPaths.Add(Asset.ToSoftObjectPath());
		}
		return SoftPaths;
	}
	return {};
}

static TSet<TWeakObjectPtr<UClass>> AsyncLoadFlags;

bool EditorAsyncLoad(UClass* Type, const FString& Dir, bool bOnce)
{
	if (GIsEditor && Type && (!bOnce || !AsyncLoadFlags.Contains(Type)) && !IsRunningCommandlet())
	{
		AsyncLoadFlags.Add(Type);
		TArray<FSoftObjectPath> SoftPaths = EditorSyncScan(Type, Dir);
		if (SoftPaths.Num() > 0)
		{
			UAssetManager::GetStreamableManager().RequestAsyncLoad(SoftPaths, FStreamableDelegate::CreateLambda([] {}), 100, true);
		}
		return true;
	}

	return false;
}

bool PinTypeFromString(FString TypeString, FEdGraphPinType& OutPinType, bool bInTemplate, bool bInContainer)
{
	TypeString.TrimStartAndEndInline();
	static auto GetPinType = [](FName Category, UObject* SubCategoryObj = nullptr) {
		auto Type = FEdGraphPinType(Category, NAME_None, SubCategoryObj, EPinContainerType::None, false, FEdGraphTerminalType());
		return Type;
	};

	static auto GetObjectPinType = [](bool bWeak = false, UObject* SubCategoryObj = UObject::StaticClass()) {
		auto Type = FEdGraphPinType(UEdGraphSchema_K2::PC_Object, NAME_None, SubCategoryObj, EPinContainerType::None, false, FEdGraphTerminalType());
		Type.bIsWeakPointer = bWeak;
		return Type;
	};

	// clang-format off
	static TMap<FName, FEdGraphPinType> NativeMap{
		{TEXT("bool"), GetPinType(UEdGraphSchema_K2::PC_Boolean)},
		{TEXT("byte"), GetPinType(UEdGraphSchema_K2::PC_Byte)},
		{TEXT("char"), GetPinType(UEdGraphSchema_K2::PC_Byte)},
		{TEXT("uint8"), GetPinType(UEdGraphSchema_K2::PC_Byte)},
		{TEXT("int8"), GetPinType(UEdGraphSchema_K2::PC_Byte)},

		{TEXT("int"), GetPinType(UEdGraphSchema_K2::PC_Int)},
		{TEXT("int16"), GetPinType(UEdGraphSchema_K2::PC_Int)},
		{TEXT("uint16"), GetPinType(UEdGraphSchema_K2::PC_Int)},
		{TEXT("int32"), GetPinType(UEdGraphSchema_K2::PC_Int)},
		{TEXT("uint32"), GetPinType(UEdGraphSchema_K2::PC_Int)},
	#if ENGINE_MINOR_VERSION >= 22
		{TEXT("int64"), GetPinType(UEdGraphSchema_K2::PC_Int64)},
		{TEXT("uint64"), GetPinType(UEdGraphSchema_K2::PC_Int64)},
	#endif
		{TEXT("float"), GetPinType(UEdGraphSchema_K2::PC_Float)},
#if 0
		{TEXT("double"), GetPinType(UEdGraphSchema_K2::PC_Double)},
#endif
		{TEXT("delegate"), GetPinType(UEdGraphSchema_K2::PC_Delegate)},
		{TEXT("name"), GetPinType(UEdGraphSchema_K2::PC_Name)},
		{TEXT("text"), GetPinType(UEdGraphSchema_K2::PC_Text)},
		{TEXT("string"), GetPinType(UEdGraphSchema_K2::PC_String)},
		{TEXT("weakobjectptr"), GetObjectPinType(true)},
		{TEXT("object"), GetObjectPinType(false)},
		{TEXT("softobjectpath"), GetPinType(UEdGraphSchema_K2::PC_SoftObject, UObject::StaticClass())},
		{TEXT("class"), GetPinType(UEdGraphSchema_K2::PC_Class, UObject::StaticClass())},
		{TEXT("softclasspath"), GetPinType(UEdGraphSchema_K2::PC_SoftClass, UObject::StaticClass())},
		{TEXT("scriptinterface"), GetPinType(UEdGraphSchema_K2::PC_Interface, UInterface::StaticClass())},

	};
	// clang-format on

	if (auto Find = NativeMap.Find(*TypeString.ToLower()))
	{
		OutPinType = *Find;
		return true;
	}

	do
	{
		if (bInTemplate)
			break;

		if (TypeString.StartsWith(TEXT("T")) && TypeString.EndsWith(TEXT(">")))
		{
			static auto GetPatternImpl = [](const TCHAR* Template, auto&& Lambda) {
				static auto Pattern = FRegexPattern(FString::Printf(TEXT("%s\\s*<\\s*(.+)"), Template));
				return Pattern;
			};

			struct FMyRegexMatcher : public FRegexMatcher
			{
				explicit operator bool() { return FindNext(); }
				FMyRegexMatcher(const FRegexPattern& Pattern, const FString& Input)
					: FRegexMatcher(Pattern, Input)
				{
				}
				operator FString() { return GetCaptureGroup(1); }
			};

			// clang-format off
#	define GS_GET_MATCHER(x) FMyRegexMatcher(GetPatternImpl(TEXT(#x), [] {}), TypeString.LeftChop(1).TrimEnd())
			// clang-format on
#	define GS_MATCH_PATTERN(x, t, c) PinTypeFromString(GS_GET_MATCHER(x), OutPinType, t, c)
			if (GS_MATCH_PATTERN(TEnumAsByte, true, false))
			{
				ensure(false);
				// OutPinType.PinCategory = UEdGraphSchema_K2::PC_Enum;
			}
			else if (GS_MATCH_PATTERN(TSubclassOf, true, false))
			{
				OutPinType.PinCategory = UEdGraphSchema_K2::PC_Class;
			}
			else if (GS_MATCH_PATTERN(TSoftClassPtr, true, false))
			{
				OutPinType.PinCategory = UEdGraphSchema_K2::PC_SoftClass;
			}
			else if (GS_MATCH_PATTERN(TSoftObjectPtr, true, false))
			{
				OutPinType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
			}
			else if (GS_MATCH_PATTERN(TWeakObjectPtr, true, false))
			{
				OutPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
				OutPinType.bIsWeakPointer = true;
			}
			else if (GS_MATCH_PATTERN(TScriptInterface, true, false))
			{
				OutPinType.PinCategory = UEdGraphSchema_K2::PC_Interface;
			}
			else if (!bInContainer)
			{
				if (GS_MATCH_PATTERN(TArray, false, true))
				{
					OutPinType.ContainerType = EPinContainerType::Array;
				}
				else if (GS_MATCH_PATTERN(TSet, false, true))
				{
					OutPinType.ContainerType = EPinContainerType::Set;
				}
				else if (auto Matcher = GS_GET_MATCHER(TMap))
				{
					FString Left;
					FString Right;
					if (!FString(Matcher).Split(TEXT(","), &Left, &Right))
						break;
					if (!PinTypeFromString(Left, OutPinType, false, true))
						break;
					FEdGraphPinType ValueType;
					if (!PinTypeFromString(Right, ValueType, false, true))
						break;
					OutPinType.PinValueType = FEdGraphTerminalType::FromPinType(ValueType);
					OutPinType.ContainerType = EPinContainerType::Map;
				}
			}
			return true;
#	undef GS_MATCH_PATTERN
#	undef GS_GET_MATCHER
		}

		if (auto Class = DynamicClass(TypeString))
		{
			OutPinType = GetPinType(UEdGraphSchema_K2::PC_Object, Class);
		}
		else if (auto Struct = DynamicStruct(TypeString))
		{
			OutPinType = GetPinType(UEdGraphSchema_K2::PC_Struct, Struct);
		}
		else if (auto Enum = DynamicEnum(TypeString))
		{
			OutPinType = GetPinType(UEdGraphSchema_K2::PC_Byte, Enum);
		}
		else
		{
			break;
		}
		return true;
	} while (0);
	OutPinType = GetPinType(UEdGraphSchema_K2::PC_Wildcard);
	return false;
}

FString GetDefaultValueOnType(const FEdGraphPinType& PinType)
{
	FString NewValue;
	// Create a useful default value based on the pin type
	if (PinType.IsContainer())
	{
		NewValue = FString();
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int)
	{
		NewValue = TEXT("0");
	}
#	if ENGINE_MINOR_VERSION >= 22
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int64)
	{
		NewValue = TEXT("0");
	}
#	endif
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Byte)
	{
		UEnum* EnumPtr = Cast<UEnum>(PinType.PinSubCategoryObject.Get());
		if (EnumPtr)
		{
			// First element of enum can change. If the enum is { A, B, C } and the default value is A,
			// the defult value should not change when enum will be changed into { N, A, B, C }
			NewValue = FString();
		}
		else
		{
			NewValue = TEXT("0");
		}
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Float)
	{
		// This is a slightly different format than is produced by PropertyValueToString, but changing it has backward compatibility issues
		NewValue = TEXT("0.0");
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
	{
		NewValue = TEXT("false");
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Name)
	{
		NewValue = TEXT("None");
	}
	else if ((PinType.PinCategory == UEdGraphSchema_K2::PC_Struct) && ((PinType.PinSubCategoryObject == TBaseStructure<FVector>::Get()) || (PinType.PinSubCategoryObject == TBaseStructure<FRotator>::Get())))
	{
		// This is a slightly different format than is produced by PropertyValueToString, but changing it has backward compatibility issues
		NewValue = TEXT("0, 0, 0");
	}
	return NewValue;
}

//////////////////////////////////////////////////////////////////////////

void* GetPropertyAddress(const TSharedPtr<IPropertyHandle>& PropertyHandle, UObject* Outer, void** ContainerAddr)
{
	if (!PropertyHandle.IsValid())
		return nullptr;
	if (!Outer)
	{
		TArray<UObject*> OuterObjects;
		PropertyHandle->GetOuterObjects(OuterObjects);
		if (OuterObjects.Num() != 1 || !OuterObjects[0])
			return nullptr;
		Outer = OuterObjects[0];
	}

	void* ValueAddress = nullptr;
	void* ParentAddress = nullptr;

	do
	{
		auto CurIndex = PropertyHandle->GetIndexInArray();
		auto ParentHandle = PropertyHandle->GetParentHandle();
		if (CurIndex >= 0)
		{
			auto ParentContainer = ParentHandle->GetProperty();
			bool IsOutmost = !ParentContainer || GetPropOwnerUObject(ParentContainer)->IsA(UClass::StaticClass());
			if (!IsOutmost)
			{
				auto ParentParentHandle = ParentHandle->GetParentHandle();
				auto ParentParentProperty = ParentParentHandle->GetProperty();

				// &Outer->[Struct...].Container[CurIndex]
				if (ensureAlways(CastField<FStructProperty>(ParentParentProperty)))
				{
					void* ParentValueAddress = GetPropertyAddress(ParentParentHandle.ToSharedRef(), Outer);
					ParentAddress = ParentParentProperty->ContainerPtrToValuePtr<void>(ParentValueAddress);
				}
			}
			else
			{
				// &Outer->Container[CurIndex]
				ParentAddress = ParentContainer->ContainerPtrToValuePtr<void>(Outer);
			}

			if (ParentHandle->AsArray())
			{
				auto ContainerProperty = CastFieldChecked<FArrayProperty>(ParentContainer);
				if (ParentAddress)
				{
					FScriptArrayHelper ArrHelper(ContainerProperty, ParentAddress);
					ValueAddress = ArrHelper.GetRawPtr(CurIndex);
				}
				break;
			}
			if (ParentHandle->AsSet())
			{
				auto ContainerProperty = CastFieldChecked<FSetProperty>(ParentContainer);
				if (ParentAddress)
				{
					FScriptSetHelper SetHelper(ContainerProperty, ParentAddress);
					ValueAddress = SetHelper.GetElementPtr(CurIndex);
				}
				break;
			}
			if (ParentHandle->AsMap())
			{
				auto ContainerProperty = CastFieldChecked<FMapProperty>(ParentContainer);
				if (ParentAddress)
				{
					FScriptMapHelper MapHelper(ContainerProperty, ParentAddress);
					ValueAddress = PropertyHandle->GetKeyHandle() ? MapHelper.GetValuePtr(CurIndex) : MapHelper.GetKeyPtr(CurIndex);
				}
				break;
			}
			ensure(false);
		}
		else
		{
			auto Property = PropertyHandle->GetProperty();
			if (ensureAlways(CastField<FStructProperty>(Property)))
			{
				bool IsOutmost = GetPropOwnerUObject(Property)->IsA(UClass::StaticClass());
				if (IsOutmost)
				{
					// &Outer->Value
					ValueAddress = Property->ContainerPtrToValuePtr<void>(Outer);
				}
				else
				{
					// &Outer->[Struct...].Value
					ParentAddress = GetPropertyAddress(ParentHandle.ToSharedRef(), Outer);
					ValueAddress = Property->ContainerPtrToValuePtr<void>(ParentAddress);
				}
			}
		}
	} while (0);

	if (ContainerAddr)
		*ContainerAddr = ParentAddress;

	return ValueAddress;
}

GENERICSTORAGES_API void* AccessPropertyAddress(const TSharedPtr<IPropertyHandle>& PropertyHandle)
{
	do
	{
		if (!PropertyHandle)
			break;

		TArray<void*> RawData;
		PropertyHandle->AccessRawData(RawData);
		if (!RawData.Num())
			break;
		return RawData[0];
	} while (0);
	return nullptr;
}

void* GetPropertyMemberPtr(const TSharedPtr<IPropertyHandle>& StructPropertyHandle, FName MemberName)
{
	do
	{
		if (!StructPropertyHandle)
			break;

		FStructProperty* StructProperty = CastField<FStructProperty>(StructPropertyHandle->GetProperty());
		if (!StructProperty)
			break;

		TArray<UObject*> OuterObjects;
		StructPropertyHandle->GetOuterObjects(OuterObjects);
		if (OuterObjects.Num() != 1)
			break;

		void* StructAddress = GetPropertyAddress(StructPropertyHandle->AsShared(), OuterObjects[0]);
		if (!StructAddress)
			break;

		auto MemberProerty = StructProperty->Struct->FindPropertyByName(MemberName);
		if (!MemberProerty)
			break;

		return MemberProerty->ContainerPtrToValuePtr<void>(StructAddress);

	} while (0);
	return nullptr;
}

void ShowNotification(const FString& Tip, bool bSucc)
{
	FNotificationInfo NotificationInfo(FText::FromString(Tip));
	NotificationInfo.FadeInDuration = 1.0f;
	NotificationInfo.FadeOutDuration = 2.0f;
	NotificationInfo.ExpireDuration = 3.0f;
	NotificationInfo.bUseLargeFont = false;

	// Add the notification to the queue
	const TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(NotificationInfo);
	Notification->SetCompletionState(bSucc ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
}

//////////////////////////////////////////////////////////////////////////
static TMap<FSoftObjectPath, TWeakObjectPtr<UScriptStruct>> TableCacheTypes;

TWeakObjectPtr<UScriptStruct> FDatatableTypePicker::FindTableType(UDataTable* Table)
{
	TWeakObjectPtr<UScriptStruct> Struct;
	if (auto Find = TableCacheTypes.Find(Table))
	{
		Struct = *Find;
	}
	return Struct;
}

TWeakObjectPtr<UScriptStruct> FDatatableTypePicker::FindTableType(TSoftObjectPtr<UDataTable> Table)
{
	return FindTableType(Table.LoadSynchronous());
}

bool FDatatableTypePicker::UpdateTableType(UDataTable* Table)
{
	if (Table)
	{
		TableCacheTypes.Emplace(FSoftObjectPath(Table), Table->RowStruct);
		return true;
	}
	return false;
}

bool FDatatableTypePicker::UpdateTableType(TSoftObjectPtr<UDataTable> Table)
{
	return UpdateTableType(Table.LoadSynchronous());
}

static TSet<FString> LoadedPaths;
bool LoadTableTypes(const FString& ScanPath)
{
	if (GIsEditor && !IsRunningCommandlet() && !LoadedPaths.Contains(ScanPath))
	{
		LoadedPaths.Add(ScanPath);
		TArray<FSoftObjectPath> SoftPaths = EditorSyncScan(UDataTable::StaticClass(), *ScanPath);
		struct FLoadScope
		{
#	if ENGINE_MINOR_VERSION >= 23
			FLoadScope()
			{
				//if (!IsLoading())
				BeginLoad(LoadContext, TEXT("DataTableScan"));
			}
			~FLoadScope()
			{
				//if (!IsLoading())
				EndLoad(LoadContext);
			}
			TRefCountPtr<FUObjectSerializeContext> LoadContext = FUObjectThreadContext::Get().GetSerializeContext();
#	else
			FLoadScope()
			{
				//if (!IsLoading())
				BeginLoad(TEXT("DataTableScan"));
			}
			~FLoadScope()
			{
				//if (!IsLoading())
				EndLoad();
			}
#	endif
		};

		FLoadScope ScopeLoad;
		for (auto& Soft : SoftPaths)
		{
			auto PackageName = Soft.GetLongPackageName();
			UPackage* DependentPackage = FindObject<UPackage>(nullptr, *PackageName, true);
			FLinkerLoad* Linker = GetPackageLinker(DependentPackage, *PackageName, LOAD_NoVerify | LOAD_Quiet | LOAD_DeferDependencyLoads, nullptr, nullptr);
			if (Linker)
			{
				UScriptStruct* Found = nullptr;
				UScriptStruct* FirstFound = nullptr;
				for (auto& a : Linker->ImportMap)
				{
					if (a.ClassName == NAME_ScriptStruct)
					{
						auto Struct = DynamicStruct(a.ObjectName.ToString());
						static auto TableBase = FTableRowBase::StaticStruct();
						if (Struct && Struct->IsChildOf(TableBase))
						{
#	if 0
							// FIXME: currently just find one type
							if (Found)
							{
								for (UScriptStruct* Type = Struct; Type && Type != TableBase; Type = Cast<UScriptStruct>(Type->GetSuperStruct()))
								{
									for (auto It = Found->Children; It; It = It->Next)
									{
										if (auto SubStruct =)
									}
								}
							}
#	endif
							if (!FirstFound)
								FirstFound = Struct;
							Found = Struct;
							// break;
						}
					}
				}

				//FIXME
				if (Found && ensure(FirstFound == Found))
					TableCacheTypes.Emplace(Soft, Found);

				if (!DependentPackage)
				{
					if (auto Package = Linker->LinkerRoot)
					{
						Linker->Detach();
					}
				}
			}
		}
		return true;
	}
	return false;
}

FDatatableTypePicker::FDatatableTypePicker(TSharedPtr<IPropertyHandle> PropertyHandle)
{
	// Default Search Path
	FilterPath = TEXT("/Game/DataTables");
	Init(PropertyHandle);
}

void FDatatableTypePicker::Init(TSharedPtr<IPropertyHandle> PropertyHandle)
{
	if (PropertyHandle.IsValid())
	{
		FString DataTableType = PropertyHandle->GetMetaData("DataTableType");
		if (!DataTableType.IsEmpty())
			ScriptStruct = FindObject<UScriptStruct>(ANY_PACKAGE, *DataTableType);
		FString DataTablePath = PropertyHandle->GetMetaData("DataTablePath");
		if (!DataTablePath.IsEmpty())
			FilterPath = DataTablePath;
		bValidate = PropertyHandle->HasMetaData("ValidateDataTable");
	}
}

TSharedRef<SWidget> FDatatableTypePicker::MakeWidget()
{
	if (ScriptStruct.IsValid())
		return SNullWidget::NullWidget;
#	if 0
	RowStructs = FDataTableEditorUtils::GetPossibleStructs();
#	else
	RowStructs.Reset();
	RowStructs.Add(nullptr);
	RowStructs.Append(FDataTableEditorUtils::GetPossibleStructs());
#	endif
	SAssignNew(MyWidget, SComboBox<UScriptStruct*>)
	.OptionsSource(&RowStructs)
	.InitiallySelectedItem(ScriptStruct.Get())
	.OnGenerateWidget_Lambda([](UScriptStruct* InStruct) {
		return SNew(STextBlock)
		.Text(InStruct ? InStruct->GetDisplayNameText() : FText::GetEmpty());
	})
	.OnSelectionChanged_Lambda([this](UScriptStruct* InStruct, ESelectInfo::Type SelectInfo) {
		if (SelectInfo == ESelectInfo::OnNavigation)
		{
			return;
		}
		if (ScriptStruct != InStruct)
		{
			ScriptStruct = InStruct;
			OnChanged.ExecuteIfBound();
		}
	})
	[
		SNew(STextBlock)
		.Text_Lambda([this]() {
			UScriptStruct* RowStruct = ScriptStruct.Get();
			return RowStruct ? RowStruct->GetDisplayNameText() : FText::GetEmpty();
		})
	];

	return MyWidget.ToSharedRef();
}

TSharedRef<SWidget> FDatatableTypePicker::MakePropertyWidget(TSharedPtr<IPropertyHandle> SoftHandle, bool bAllowClear)
{
	Init(SoftHandle);
	LoadTableTypes(FilterPath);
	TSharedPtr<SWidget> RetWidget;
	SAssignNew(RetWidget, SObjectPropertyEntryBox)
	.PropertyHandle(SoftHandle)
	.AllowedClass(UDataTable::StaticClass())
	.OnShouldSetAsset_Lambda([bValidate{this->bValidate}, Type{this->ScriptStruct}, FilterPath{this->FilterPath}](const FAssetData& AssetData) {
		if (!bValidate)
			return true;
		if (auto DataTable = Cast<UDataTable>(AssetData.GetAsset()))
		{
			FDatatableTypePicker::UpdateTableType(DataTable);
			if (!Type.IsValid() || DataTable->RowStruct == Type)
				return true;
		}
		return false;
	})
	.OnShouldFilterAsset_Lambda([Type{this->ScriptStruct}, FilterPath{this->FilterPath}](const FAssetData& AssetData) {
		bool bFilterOut = true;
		do
		{
			auto Path = AssetData.ToSoftObjectPath();
			if (FilterPath.IsEmpty() || Path.ToString().StartsWith(FilterPath))
			{
				auto DataTable = Cast<UDataTable>(Path.ResolveObject());
				if (DataTable)
				{
					FDatatableTypePicker::UpdateTableType(DataTable);
					if (Type.IsValid() && DataTable->RowStruct != Type)
						break;
					bFilterOut = false;
				}
				else
				{
					auto Find = TableCacheTypes.Find(Path);
					if (Find && Find->IsValid())
					{
						if (Type.IsValid() && *Find != Type)
							break;
						bFilterOut = false;
					}
					else
					{
						DataTable = Cast<UDataTable>(AssetData.GetAsset());
						if (!DataTable)
							break;
						FDatatableTypePicker::UpdateTableType(DataTable);
						if (Type.IsValid() && DataTable->RowStruct != Type)
							break;
						bFilterOut = false;
					}
				}
			}
		} while (0);
		return bFilterOut;
	})
	.AllowClear(bAllowClear)
	.DisplayThumbnail(false);
	return RetWidget.ToSharedRef();
}

TSharedRef<SWidget> FDatatableTypePicker::MakeDynamicPropertyWidget(TSharedPtr<IPropertyHandle> SoftHandle, bool bAllowClear)
{
	Init(SoftHandle);
	LoadTableTypes(FilterPath);
	TSharedPtr<SWidget> RetWidget;
	SAssignNew(RetWidget, SObjectPropertyEntryBox)
	.PropertyHandle(SoftHandle)
	.AllowedClass(UDataTable::StaticClass())
	.OnShouldSetAsset_Lambda([this](const FAssetData& AssetData) {
		if (!bValidate)
			return true;
		if (auto DataTable = Cast<UDataTable>(AssetData.GetAsset()))
		{
			FDatatableTypePicker::UpdateTableType(DataTable);
			if (!ScriptStruct.IsValid() || DataTable->RowStruct == ScriptStruct)
				return true;
		}
		return false;
	})
	.OnShouldFilterAsset_Lambda([this](const FAssetData& AssetData) {
		bool bFilterOut = true;
		do
		{
			auto Path = AssetData.ToSoftObjectPath();
			if (FilterPath.IsEmpty() || Path.ToString().StartsWith(FilterPath))
			{
				auto DataTable = Cast<UDataTable>(Path.ResolveObject());
				if (DataTable)
				{
					FDatatableTypePicker::UpdateTableType(DataTable);
					if (ScriptStruct.IsValid() && DataTable->RowStruct != ScriptStruct)
						break;
					bFilterOut = false;
				}
				else
				{
					auto Find = TableCacheTypes.Find(Path);
					if (Find && Find->IsValid())
					{
						if (ScriptStruct.IsValid() && *Find != ScriptStruct)
							break;
						bFilterOut = false;
					}
					else
					{
						DataTable = Cast<UDataTable>(AssetData.GetAsset());
						if (!DataTable)
							break;
						FDatatableTypePicker::UpdateTableType(DataTable);
						if (ScriptStruct.IsValid() && DataTable->RowStruct != ScriptStruct)
							break;
						bFilterOut = false;
					}
				}
			}
		} while (0);
		return bFilterOut;
	})
	.AllowClear(bAllowClear)
	.DisplayThumbnail(false);
	return RetWidget.ToSharedRef();
}

//////////////////////////////////////////////////////////////////////////
FScopedPropertyTransaction::FScopedPropertyTransaction(const TSharedPtr<IPropertyHandle>& InPropertyHandle, const FText& SessionName, const TCHAR* TransactionContext)
	: PropertyHandle(InPropertyHandle.Get())
	, Scoped(TransactionContext, SessionName, GetPropOwnerUObject(PropertyHandle->GetProperty()))
{
	PropertyHandle->NotifyPreChange();
}

FScopedPropertyTransaction::~FScopedPropertyTransaction()
{
	PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	PropertyHandle->NotifyFinishedChangingProperties();
}

}  // namespace UnrealEditorUtils
#endif
