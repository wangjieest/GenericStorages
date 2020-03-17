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

#include "GS_EditorUtils.h"

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

namespace GS_EditorUtils
{
FName GetPropertyName(FProperty* InProperty)
{
#	if 1
	using ValueType = FName (*)(FProperty * Property);
	using KeyType = const FFieldClass*;
#		define DEF_PAIR_CELL(PropertyTypeName) TPairInitializer<const KeyType&, const ValueType&>(PropertyTypeName::StaticClass(), [](FProperty* Property) { return GetPropertyName<typename PropertyTypeName::TCppType>(); })
#		define DEF_PAIR_CELL_CUSTOM(PropertyTypeName, ...) TPairInitializer<const KeyType&, const ValueType&>(PropertyTypeName::StaticClass(), [](FProperty* Property) { return __VA_ARGS__; })

	static TMap<KeyType, ValueType> DispatchMap{
		//
		DEF_PAIR_CELL_CUSTOM(FStructProperty, FName(*CastProp<FStructProperty>(Property)->Struct->GetName())),
		DEF_PAIR_CELL(FByteProperty),
		DEF_PAIR_CELL(FBoolProperty),
		DEF_PAIR_CELL(FIntProperty),
		DEF_PAIR_CELL(FUInt16Property),
		DEF_PAIR_CELL(FInt16Property),
		DEF_PAIR_CELL(FUInt32Property),
		DEF_PAIR_CELL(FInt64Property),
		DEF_PAIR_CELL(FUInt64Property),
		DEF_PAIR_CELL(FFloatProperty),
		DEF_PAIR_CELL(FDoubleProperty),
		DEF_PAIR_CELL(FStrProperty),
		DEF_PAIR_CELL(FNameProperty),
		DEF_PAIR_CELL(FTextProperty),
		DEF_PAIR_CELL_CUSTOM(UEnumProperty, GetPropertyName(CastProp<FEnumProperty>(Property)->GetUnderlyingProperty())),
		DEF_PAIR_CELL(FLazyObjectProperty),
		DEF_PAIR_CELL(FSoftObjectProperty),
		DEF_PAIR_CELL(FWeakObjectProperty),
		DEF_PAIR_CELL(FInterfaceProperty),
		DEF_PAIR_CELL_CUSTOM(FObjectProperty, GetPropertyName<UObject>()),
		DEF_PAIR_CELL_CUSTOM(FClassProperty, GetPropertyName<UObject>()),
		DEF_PAIR_CELL_CUSTOM(FSoftClassProperty, GetPropertyName<FSoftObjectPtr>()),
		DEF_PAIR_CELL_CUSTOM(FArrayProperty, GS_CLASS_TO_NAME::TTraitsTemplate<void>::GetTArrayName(*GetPropertyName(CastProp<FArrayProperty>(Property)->Inner).ToString())),
		DEF_PAIR_CELL_CUSTOM(FSetProperty, GS_CLASS_TO_NAME::TTraitsTemplate<void>::GetTSetName(*GetPropertyName(CastProp<FSetProperty>(Property)->ElementProp).ToString())),
		DEF_PAIR_CELL_CUSTOM(FMapProperty,
							 GS_CLASS_TO_NAME::TTraitsTemplate<void>::GetTMapName(*GetPropertyName(CastProp<FMapProperty>(Property)->KeyProp).ToString(), *GetPropertyName(CastProp<FMapProperty>(Property)->ValueProp).ToString()))
		//
	};
	// clang-format on

#		undef DEF_PAIR_CELL
#		undef DEF_PAIR_CELL_CUSTOM

	if (auto Func = DispatchMap.Find(InProperty->GetClass()))
	{
		return (*Func)(InProperty);
	}
#	else

#		define DispathPropertyType(PropertyTypeName) \
			else if (CastProp<PropertyTypeName>(InProperty)) { return GetPropertyName<typename PropertyTypeName::TCppType>(); }

	// clang-format off
		if (FStructProperty* SubStructProperty = CastProp<FStructProperty>(InProperty))
		{
			return *SubStructProperty->Struct->GetName();
		}
		DispathPropertyType(FByteProperty)
			DispathPropertyType(FBoolProperty)
			DispathPropertyType(FIntProperty)
			DispathPropertyType(FUInt16Property)
			DispathPropertyType(FInt16Property)
			DispathPropertyType(FUInt32Property)
			DispathPropertyType(FInt64Property)
			DispathPropertyType(FUInt64Property)
			DispathPropertyType(FFloatProperty)
			DispathPropertyType(FDoubleProperty)
			DispathPropertyType(FStrProperty)
			DispathPropertyType(FNameProperty)
			DispathPropertyType(FTextProperty)
		else if (FEnumProperty* SubEnumProperty = CastProp<FEnumProperty>(InProperty))
		{
			return GetPropertyName(SubEnumProperty->GetUnderlyingProperty());
		}
	// clang-format on
	else if (CastProp<FObjectPropertyBase>(InProperty))
	{
		// [PropertyClass] is the detail type of target
		// Weak
		if (CastProp<FWeakObjectProperty>(InProperty))
		{
			// FWeakObjectPtr
			return GetPropertyName<FWeakObjectProperty::TCppType>();
		}
		// Object
		// 		else if (CastProp<FClassProperty>(InProperty))
		// 		{
		// 			// UClassProperty is a specialization of UObjectProperty with MetaClass
		// 			return GetPropertyName<UObject>();
		// 		}
		else if (CastProp<FObjectProperty>(InProperty))
		{
			// UObject*
			return GetPropertyName<UObject>();
		}
		// SoftObject
		// 		else if (CastProp<FSoftClassProperty>(InProperty))
		// 		{
		// 			// USoftClassProperty is a specialization of USoftObjectProperty with MetaClass
		// 			return GetPropertyName<FSoftObjectPtr>();
		// 		}
		else if (CastProp<FSoftObjectProperty>(InProperty))
		{
			// FSoftObjectPtr
			return GetPropertyName<FSoftObjectProperty::TCppType>();
		}
		// Lazy
		else if (CastProp<FLazyObjectProperty>(InProperty))
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
	else if (FArrayProperty* SubArrProperty = CastProp<FArrayProperty>(InProperty)) { return GS_CLASS_TO_NAME::TTraitsTemplate<void>::GetTArrayName(*GetPropertyName(SubArrProperty->Inner).ToString()); }
	else if (FMapProperty* SubMapProperty = CastProp<FMapProperty>(InProperty))
	{
		return GS_CLASS_TO_NAME::TTraitsTemplate<void>::GetTMapName(*GetPropertyName(SubMapProperty->KeyProp).ToString(), *GetPropertyName(SubMapProperty->ValueProp).ToString());
	}
	else if (FSetProperty* SubSetProperty = CastProp<FSetProperty>(InProperty)) { return GS_CLASS_TO_NAME::TTraitsTemplate<void>::GetTSetName(*GetPropertyName(SubSetProperty->ElementProp).ToString()); }
	else if (FInterfaceProperty* SubIncProperty = CastProp<FInterfaceProperty>(InProperty))
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

FName GetPropertyName(FProperty* InProperty, EGSPropertyClass PropertyEnum, EGSPropertyClass ValueEnum, EGSPropertyClass KeyEnum)
{
	using KeyType = EGSPropertyClass;
	using ValueType = FName (*)(FProperty * Property, EGSPropertyClass, EGSPropertyClass);
#	define DEF_PAIR_CELL(Index) \
		TPairInitializer<const KeyType&, const ValueType&>(KeyType::Index, [](FProperty* Property, EGSPropertyClass InValueEnum, EGSPropertyClass InKeyEnum) { return GetPropertyName<typename F##Index##Property ::TCppType>(); })

#	define DEF_PAIR_CELL_CUSTOM(Index, ...) TPairInitializer<const KeyType&, const ValueType&>(KeyType::Index, [](FProperty* Property, EGSPropertyClass InValueEnum, EGSPropertyClass InKeyEnum) { return __VA_ARGS__; })

	static TMap<KeyType, ValueType> DispatchMap{
		//
		DEF_PAIR_CELL(Byte),
		DEF_PAIR_CELL(Int8),
		DEF_PAIR_CELL(Int16),
		DEF_PAIR_CELL(Int),
		DEF_PAIR_CELL(UInt64),
		DEF_PAIR_CELL(UInt16),
		DEF_PAIR_CELL(UnsizedInt),
		DEF_PAIR_CELL(UnsizedUInt),
		DEF_PAIR_CELL(Double),
		DEF_PAIR_CELL(Float),
		DEF_PAIR_CELL(Bool),
		DEF_PAIR_CELL(SoftClass),
		DEF_PAIR_CELL(WeakObject),
		DEF_PAIR_CELL(LazyObject),
		DEF_PAIR_CELL(SoftObject),
		DEF_PAIR_CELL(Interface),
		DEF_PAIR_CELL(Name),
		DEF_PAIR_CELL(Str),
		DEF_PAIR_CELL_CUSTOM(Object, GetPropertyName<UObject>()),
		DEF_PAIR_CELL_CUSTOM(Class, GetPropertyName<UObject>()),
		DEF_PAIR_CELL_CUSTOM(Array, GS_CLASS_TO_NAME::TTraitsTemplate<void>::GetTArrayName(*GetPropertyName(CastProp<FArrayProperty>(Property)->Inner, InValueEnum).ToString())),
		DEF_PAIR_CELL_CUSTOM(
			Map,
			GS_CLASS_TO_NAME::TTraitsTemplate<void>::GetTMapName(*GetPropertyName(CastProp<FMapProperty>(Property)->KeyProp, InKeyEnum).ToString(), *GetPropertyName(CastProp<FMapProperty>(Property)->ValueProp, InValueEnum).ToString())),
		DEF_PAIR_CELL_CUSTOM(Set, GS_CLASS_TO_NAME::TTraitsTemplate<void>::GetTSetName(*GetPropertyName(CastProp<FSetProperty>(Property)->ElementProp, InValueEnum).ToString())),
		DEF_PAIR_CELL_CUSTOM(Struct, FName(*CastProp<FStructProperty>(Property)->Struct->GetName())),
		DEF_PAIR_CELL(Delegate),
		//DEF_PAIR_CELL(InlineMulticastDelegate),
		DEF_PAIR_CELL(Text),
		DEF_PAIR_CELL_CUSTOM(Enum, GetPropertyName(CastProp<FEnumProperty>(Property)->GetUnderlyingProperty()))};

#	undef DEF_PAIR_CELL
#	undef DEF_PAIR_CELL_CUSTOM

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
		{TEXT("uint8"), GetPinType(UEdGraphSchema_K2::PC_Byte)},
		{TEXT("int8"), GetPinType(UEdGraphSchema_K2::PC_Byte)},
		{TEXT("char"), GetPinType(UEdGraphSchema_K2::PC_Byte)},
		{TEXT("int"), GetPinType(UEdGraphSchema_K2::PC_Int)},
		{TEXT("int32"), GetPinType(UEdGraphSchema_K2::PC_Int)},
		{TEXT("uint32"), GetPinType(UEdGraphSchema_K2::PC_Int)},
		{TEXT("byte"), GetPinType(UEdGraphSchema_K2::PC_Byte)},
		{TEXT("bool"), GetPinType(UEdGraphSchema_K2::PC_Boolean)},
		{TEXT("float"), GetPinType(UEdGraphSchema_K2::PC_Float)},
		{TEXT("name"), GetPinType(UEdGraphSchema_K2::PC_Name)},
		{TEXT("text"), GetPinType(UEdGraphSchema_K2::PC_Text)},
		{TEXT("string"), GetPinType(UEdGraphSchema_K2::PC_String)},
		{TEXT("weakobjectptr"), GetObjectPinType(true)},
		{TEXT("object"), GetObjectPinType(false)},
		{TEXT("softobjectpath"), GetPinType(UEdGraphSchema_K2::PC_SoftObject, UObject::StaticClass())},
		{TEXT("class"), GetPinType(UEdGraphSchema_K2::PC_Class, UObject::StaticClass())},
		{TEXT("softclasspath"), GetPinType(UEdGraphSchema_K2::PC_SoftClass, UObject::StaticClass())},
		{TEXT("scriptinterface"), GetPinType(UEdGraphSchema_K2::PC_Interface, UInterface::StaticClass())},
	#if ENGINE_MINOR_VERSION >= 22
		{TEXT("int64"), GetPinType(UEdGraphSchema_K2::PC_Int64)},
	#endif
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

#	define GET_PATTERN(x) auto Matecher = FMyRegexMatcher(GetPatternImpl(TEXT(x), [] {}), TypeString.LeftChop(1).TrimEnd())

			if (GET_PATTERN("TEnumAsByte"))
			{
				if (!PinTypeFromString(Matecher, OutPinType, true))
					break;
			}
			if (GET_PATTERN("TSubclassOf"))
			{
				if (PinTypeFromString(Matecher, OutPinType, true))
					OutPinType.PinCategory = UEdGraphSchema_K2::PC_Class;
				else
					break;
			}
			if (GET_PATTERN("TSoftClassPtr"))
			{
				if (PinTypeFromString(Matecher, OutPinType, true))
					OutPinType.PinCategory = UEdGraphSchema_K2::PC_SoftClass;
				else
					break;
			}
			if (GET_PATTERN("TSoftObjectPtr"))
			{
				if (PinTypeFromString(Matecher, OutPinType, true))
					OutPinType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
				else
					break;
			}
			if (GET_PATTERN("TWeakObjectPtr"))
			{
				if (PinTypeFromString(Matecher, OutPinType, true))
				{
					OutPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
					OutPinType.bIsWeakPointer = true;
				}
				else
					break;
			}
			if (GET_PATTERN("TScriptInterface"))
			{
				if (PinTypeFromString(Matecher, OutPinType, true))
					OutPinType.PinCategory = UEdGraphSchema_K2::PC_Interface;
				else
					break;
			}
			if (!bInContainer)
			{
				if (GET_PATTERN("TArray"))
				{
					if (PinTypeFromString(Matecher, OutPinType, false, true))
						OutPinType.ContainerType = EPinContainerType::Array;
					else
						break;
				}
				if (GET_PATTERN("TSet"))
				{
					if (PinTypeFromString(Matecher, OutPinType, false, true))
						OutPinType.ContainerType = EPinContainerType::Set;
					else
						break;
				}
				if (GET_PATTERN("TMap"))
				{
					FString Left;
					FString Right;
					if (!FString(Matecher).Split(TEXT(","), &Left, &Right))
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
#	undef GET_PATTERN
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

void* GetStructPropertyAddress(const TSharedPtr<IPropertyHandle>& PropertyHandle, UObject* Outer, void** ContainerAddr)
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
			bool HasParent = !!ParentContainer;
			bool IsOutmost = HasParent ? GetOwnerUObject(ParentContainer)->IsA(UClass::StaticClass()) : true;
			if (!IsOutmost)
			{
				auto ParentParentHandle = ParentHandle->GetParentHandle();
				auto ParentParentProperty = ParentParentHandle->GetProperty();
				if (ensure(CastProp<FStructProperty>(ParentParentProperty)))
				{
					void* ParentValueAddress = GetStructPropertyAddress(ParentParentHandle.ToSharedRef(), Outer);
					ParentAddress = ParentParentProperty->ContainerPtrToValuePtr<void>(ParentValueAddress);
				}
			}
			if (ParentHandle->AsArray())
			{
				auto ContainerProperty = CastProp<FArrayProperty>(ParentContainer);
				if (IsOutmost)
				{
					// Outer->Container[CurIndex]
					ParentAddress = ContainerProperty->ContainerPtrToValuePtr<void>(Outer);
				}
				else
				{
					// Outer->[Struct...]->Container[CurIndex]
				}
				if (ParentAddress)
				{
					FScriptArrayHelper ArrHelper(ContainerProperty, ParentAddress);
					ValueAddress = ArrHelper.GetRawPtr(CurIndex);
				}

				break;
			}
			if (ParentHandle->AsSet())
			{
				auto ContainerProperty = CastProp<FSetProperty>(ParentContainer);
				if (IsOutmost)
				{
					// Outer->Container[CurIndex]
					ParentAddress = ContainerProperty->ContainerPtrToValuePtr<void>(Outer);
				}
				else
				{
					// Outer->[Struct...]->Container[CurIndex]
				}
				if (ParentAddress)
				{
					FScriptSetHelper SetHelper(ContainerProperty, ParentAddress);
					ValueAddress = SetHelper.GetElementPtr(CurIndex);
				}

				break;
			}
			if (ParentHandle->AsMap())
			{
				auto ContainerProperty = CastProp<FMapProperty>(ParentContainer);

				// FIXME Outer->Container[CurIndex].Key
				auto KeyHandle = ParentHandle->GetKeyHandle();

				if (IsOutmost)
				{
					// Outer->Container[CurIndex].Value
					ParentAddress = ContainerProperty->ContainerPtrToValuePtr<void>(Outer);
				}
				else
				{
					// Outer->[Struct...]->Container[CurIndex].Value
				}
				if (ParentAddress)
				{
					FScriptMapHelper MapHelper(ContainerProperty, ParentAddress);
					ValueAddress = MapHelper.GetValuePtr(CurIndex);
				}

				break;
			}
			ensure(false);
		}
		else
		{
			auto Property = PropertyHandle->GetProperty();
			if (ensure(CastProp<FStructProperty>(Property)))
			{
				bool IsOutmost = GetOwnerUObject(Property)->IsA(UClass::StaticClass());
				if (IsOutmost)
				{
					// Outer->Struct
					ValueAddress = Property->ContainerPtrToValuePtr<void>(Outer);
				}
				else
				{
					// Outer->[Struct...]->Struct
					ParentAddress = GetStructPropertyAddress(ParentHandle.ToSharedRef(), Outer);
					ValueAddress = Property->ContainerPtrToValuePtr<void>(ParentAddress);
				}
			}
		}
	} while (0);

	if (ContainerAddr)
		*ContainerAddr = ParentAddress;

	return ValueAddress;
}

void* GetStructPropertyValuePtr(const TSharedPtr<IPropertyHandle>& StructPropertyHandle, FName MemberName)
{
	do
	{
		TArray<UObject*> OuterObjects;
		StructPropertyHandle->GetOuterObjects(OuterObjects);
		if (OuterObjects.Num() != 1)
			break;

		FStructProperty* StructProperty = CastProp<FStructProperty>(StructPropertyHandle->GetProperty());
		if (!StructProperty)
			break;

		void* StructAddress = GetStructPropertyAddress(StructPropertyHandle->AsShared(), OuterObjects[0]);
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
	, Scoped(TransactionContext, SessionName, GetOwnerUObject(PropertyHandle->GetProperty()))
{
	PropertyHandle->NotifyPreChange();
}

FScopedPropertyTransaction::~FScopedPropertyTransaction()
{
	PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	PropertyHandle->NotifyFinishedChangingProperties();
}

}  // namespace GS_EditorUtils
#endif
