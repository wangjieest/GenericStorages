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

#include "Containers/EnumAsByte.h"
#include "GS_TypeTraits.h"
#include "HAL/PlatformAtomics.h"
#include "Misc/AssertionMacros.h"
#include "Runtime/GameplayTasks/Public/GameplayTaskTypes.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Templates/SubclassOf.h"
#include "UObject/LazyObjectPtr.h"
#include "UObject/Object.h"
#include "UObject/ScriptInterface.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/WeakObjectPtr.h"

#if !defined(GS_CLASS_TO_NAME)
#	define GS_CLASS_TO_NAME GS_Class2Name
#endif

namespace GS_CLASS_TO_NAME
{
// clang-format off
	template <typename T> struct TManualGeneratedName
	{
		// FORCEINLINE static const TCHAR* GetName() { return TEXT("UnHandledType"); }
		enum
		{
			dispatch_value = 1,
			value = 0
		};
	};
// clang-format on

#define GS_MANUAL_GENERATE_NAME(TYPE, NAME)                              \
	template<>                                                           \
	struct TManualGeneratedName<TYPE>                                    \
	{                                                                    \
		enum                                                             \
		{                                                                \
			value = TManualGeneratedName<void>::dispatch_value,          \
		};                                                               \
		FORCEINLINE static const TCHAR* GetName() { return TEXT(NAME); } \
	};

// base props

#define GS_MANUAL_NAME_OF(Class) GS_MANUAL_GENERATE_NAME(Class, #Class)
GS_MANUAL_NAME_OF(char)
GS_MANUAL_NAME_OF(wchar_t)
GS_MANUAL_NAME_OF(bool)
GS_MANUAL_NAME_OF(int8)
GS_MANUAL_NAME_OF(uint8)
GS_MANUAL_NAME_OF(int16)
GS_MANUAL_NAME_OF(uint16)
GS_MANUAL_NAME_OF(int32)
GS_MANUAL_NAME_OF(uint32)
GS_MANUAL_NAME_OF(int64)
GS_MANUAL_NAME_OF(uint64)
GS_MANUAL_NAME_OF(float)
GS_MANUAL_NAME_OF(double)
#undef GS_MANUAL_NAME_OF

// custom def
#define GS_MANUAL_NAME_OF(Class)               \
	namespace GS_CLASS_TO_NAME                 \
	{                                          \
		GS_MANUAL_GENERATE_NAME(Class, #Class) \
	}

GS_MANUAL_GENERATE_NAME(FString, "String")
GS_MANUAL_GENERATE_NAME(FName, "Name")
GS_MANUAL_GENERATE_NAME(FText, "Text")

GS_MANUAL_GENERATE_NAME(UObject, "Object")

// THasBaseStructure
template<typename T>
struct THasBaseStructure
{
	enum
	{
		value = 0,
	};
};
#define GS_MANUAL_GENERATE_STRUCT_NAME(TYPE, NAME) \
	GS_MANUAL_GENERATE_NAME(TYPE, NAME)            \
	template<>                                     \
	struct THasBaseStructure<TYPE>                 \
	{                                              \
		enum                                       \
		{                                          \
			value = 1                              \
		};                                         \
	};

// provided by TBaseStructure
GS_MANUAL_GENERATE_STRUCT_NAME(FRotator, "Rotator")
GS_MANUAL_GENERATE_STRUCT_NAME(FQuat, "Quat")
GS_MANUAL_GENERATE_STRUCT_NAME(FTransform, "Transform")
GS_MANUAL_GENERATE_STRUCT_NAME(FLinearColor, "LinearColor")
GS_MANUAL_GENERATE_STRUCT_NAME(FColor, "Color")
GS_MANUAL_GENERATE_STRUCT_NAME(FPlane, "Plane")
GS_MANUAL_GENERATE_STRUCT_NAME(FVector, "Vector")
GS_MANUAL_GENERATE_STRUCT_NAME(FVector2D, "Vector2D")
GS_MANUAL_GENERATE_STRUCT_NAME(FVector4, "Vector4")
GS_MANUAL_GENERATE_STRUCT_NAME(FRandomStream, "RandomStream")
GS_MANUAL_GENERATE_STRUCT_NAME(FGuid, "Guid")
GS_MANUAL_GENERATE_STRUCT_NAME(FBox2D, "Box2D")
GS_MANUAL_GENERATE_STRUCT_NAME(FFallbackStruct, "FallbackStruct")
GS_MANUAL_GENERATE_STRUCT_NAME(FFloatRangeBound, "FloatRangeBound")
GS_MANUAL_GENERATE_STRUCT_NAME(FFloatRange, "FloatRange")
GS_MANUAL_GENERATE_STRUCT_NAME(FInt32RangeBound, "Int32RangeBound")
GS_MANUAL_GENERATE_STRUCT_NAME(FInt32Range, "Int32Range")
GS_MANUAL_GENERATE_STRUCT_NAME(FFloatInterval, "FloatInterval")
GS_MANUAL_GENERATE_STRUCT_NAME(FInt32Interval, "Int32Interval")
GS_MANUAL_GENERATE_STRUCT_NAME(FSoftObjectPath, "SoftObjectPath")
GS_MANUAL_GENERATE_STRUCT_NAME(FSoftClassPath, "SoftClassPath")
GS_MANUAL_GENERATE_STRUCT_NAME(FPrimaryAssetType, "PrimaryAssetType")
GS_MANUAL_GENERATE_STRUCT_NAME(FPrimaryAssetId, "PrimaryAssetId")
GS_MANUAL_GENERATE_STRUCT_NAME(FDateTime, "DateTime")
GS_MANUAL_GENERATE_STRUCT_NAME(FPolyglotTextData, "PolyglotTextData")

// detect UCLASS
template<typename T>
struct THasUObjectClass
{
	enum
	{
		dispatch_value = 2,
		value = std::is_base_of<UObject, std::decay_t<std::remove_pointer_t<T>>>::value ? dispatch_value : 0
	};
	using decay_type = std::decay_t<std::remove_pointer_t<T>>;
};

// detect USTRUCT
template<typename T>
struct THasStaticStruct
{
	template<typename V>
	using HasStaticStructType = decltype(V::StaticStruct());
	template<typename V>
	using HasStaticStruct = GS_TYPE_TRAITS::IsDetected<HasStaticStructType, V>;
	enum
	{
		dispatch_value = 3,
		value = (HasStaticStruct<T>::value || THasBaseStructure<T>::value) ? dispatch_value : 0
	};
};

// enum to number
template<typename T, bool b = std::is_enum<T>::value>
struct TTraitsEnum
{
	enum
	{
		dispatch_value = 4,
		value = (b ? dispatch_value : 0)
	};
	using type = std::decay_t<T>;
};
template<typename T>
struct TTraitsEnum<T, true>
{
	enum
	{
		dispatch_value = TTraitsEnum<void>::dispatch_value,
		value = dispatch_value
	};
	using type = std::underlying_type_t<std::decay_t<T>>;
};

template<typename T>
struct TTraitsTemplate
{
	enum
	{
		dispatch_value = 5,
		value = 0,
		nested = 0,
	};
	static FName GetTArrayName(const TCHAR* Inner) { return *FString::Printf(TEXT("TArray<%s>"), Inner); }
	static FName GetTMapName(const TCHAR* InnerKey, const TCHAR* InnerValue) { return *FString::Printf(TEXT("TMap<%s,%s>"), InnerKey, InnerValue); }
	static FName GetTSetName(const TCHAR* Inner) { return *FString::Printf(TEXT("TSet<%s>"), Inner); }
};

// detect TArray
template<typename InElementType, typename InAllocator>
struct TTraitsTemplate<TArray<InElementType, InAllocator>>
{
	static_assert(std::is_same<InAllocator, FDefaultAllocator>::value, "only support FDefaultAllocator");
	static_assert(!TTraitsTemplate<InElementType>::nested, "not support nested container");
	enum
	{
		value = TTraitsTemplate<void>::dispatch_value,
		nested = 1
	};
	static auto GetName();
};

// detect TMap
template<typename InKeyType, typename InValueType, typename SetAllocator, typename KeyFuncs>
struct TTraitsTemplate<TMap<InKeyType, InValueType, SetAllocator, KeyFuncs>>
{
	static_assert(std::is_same<SetAllocator, FDefaultSetAllocator>::value, "only support FDefaultSetAllocator");
	static_assert(std::is_same<KeyFuncs, TDefaultMapHashableKeyFuncs<InKeyType, InValueType, false>>::value, "only support TDefaultMapHashableKeyFuncs");
	static_assert(!TTraitsTemplate<InKeyType>::nested && !TTraitsTemplate<InValueType>::nested, "not support nested container");
	enum
	{
		value = TTraitsTemplate<void>::dispatch_value,
		nested = 1
	};
	static auto GetName();
};

// detect TSet
template<typename InElementType, typename KeyFuncs, typename InAllocator>
struct TTraitsTemplate<TSet<InElementType, KeyFuncs, InAllocator>>
{
	static_assert(std::is_same<KeyFuncs, DefaultKeyFuncs<InElementType>>::value, "only support DefaultKeyFuncs");
	static_assert(std::is_same<InAllocator, FDefaultSetAllocator>::value, "only support FDefaultSetAllocator");
	static_assert(!TTraitsTemplate<InElementType>::nested, "not support nested container");
	enum
	{
		value = TTraitsTemplate<void>::dispatch_value,
		nested = 1
	};
	static auto GetName();
};

/*
TMulticastScriptDelegate<T> 
TScriptDelegate<T>
*/
GS_MANUAL_GENERATE_NAME(FScriptDelegate, "ScriptDelegate")
template<>
struct TTraitsTemplate<FScriptDelegate>
{
	enum
	{
		value = TTraitsTemplate<void>::dispatch_value,
	};
	static auto GetName() { return TManualGeneratedName<FScriptDelegate>::GetName(); }
};
template<typename T>
struct TTraitsTemplate<TScriptDelegate<T>> : TTraitsTemplate<FScriptDelegate>
{
};

GS_MANUAL_GENERATE_NAME(FMulticastScriptDelegate, "MulticastScriptDelegate")
template<>
struct TTraitsTemplate<FMulticastScriptDelegate>
{
	enum
	{
		value = TTraitsTemplate<void>::dispatch_value,
	};
	static auto GetName() { return TManualGeneratedName<FMulticastScriptDelegate>::GetName(); }
};
template<typename T>
struct TTraitsTemplate<TMulticastScriptDelegate<T>> : TTraitsTemplate<FMulticastScriptDelegate>
{
};

/*
FWeakObjectPtr/TWeakObjectPtr<T>
{
 int32	ObjectIndex;
 int32	ObjectSerialNumber;
}
*/
GS_MANUAL_GENERATE_NAME(FWeakObjectPtr, "WeakObjectPtr")
template<>
struct TTraitsTemplate<FWeakObjectPtr>
{
	enum
	{
		value = TTraitsTemplate<void>::dispatch_value,
	};
	static auto GetName() { return TManualGeneratedName<FWeakObjectPtr>::GetName(); }
};
template<typename T>
struct TTraitsTemplate<TWeakObjectPtr<T>> : TTraitsTemplate<FWeakObjectPtr>
{
};

/*
FScriptInterface/TScriptInterface<T>
{
 UObject*	ObjectPointer;
 void*	InterfacePointer;
}
*/
GS_MANUAL_GENERATE_NAME(FScriptInterface, "ScriptInterface")
template<>
struct TTraitsTemplate<FScriptInterface>
{
	enum
	{
		value = TTraitsTemplate<void>::dispatch_value,
	};
	static auto GetName() { return TManualGeneratedName<FScriptInterface>::GetName(); }
};
template<typename T>
struct TTraitsTemplate<TScriptInterface<T>> : public TTraitsTemplate<FScriptInterface>
{
};

/*
FSoftObjectPtr/TSoftObjectPtr<T>/TSoftClassPtr<T>/TAssetPtr<T>/TAssetSubclassOf<T>
{
 mutable FWeakObjectPtr	WeakPtr;
 mutable int32	TagAtLastTest;
 FSoftObjectPath	ObjectID;
}
*/
GS_MANUAL_GENERATE_NAME(FSoftObjectPtr, "SoftObjectPtr")
template<>
struct TTraitsTemplate<FSoftObjectPtr>
{
	enum
	{
		value = TTraitsTemplate<void>::dispatch_value,
	};
	static auto GetName() { return TManualGeneratedName<FSoftObjectPtr>::GetName(); }
};

template<typename T>
struct TTraitsTemplate<TSoftObjectPtr<T>> : public TTraitsTemplate<FSoftObjectPtr>
{
};
template<typename T>
struct TTraitsTemplate<TSoftClassPtr<T>> : public TTraitsTemplate<FSoftObjectPtr>
{
};

/*
FLazyObjectPtr/TLazyObjectPtr<T>
{
	mutable FWeakObjectPtr	WeakPtr;
	mutable int32	TagAtLastTest;
	FUniqueObjectGuid	ObjectID;
}
*/
GS_MANUAL_GENERATE_NAME(FLazyObjectPtr, "LazyObjectPtr")
template<>
struct TTraitsTemplate<FLazyObjectPtr>
{
	enum
	{
		value = TTraitsTemplate<void>::dispatch_value,
	};
	static auto GetName() { return TManualGeneratedName<FLazyObjectPtr>::GetName(); }
};
template<typename T>
struct TTraitsTemplate<TLazyObjectPtr<T>> : public TTraitsTemplate<FLazyObjectPtr>
{
};

/*
TSubclassOf<T>
{
	UClass* Class;
}
*/
template<typename T>
struct TTraitsTemplate<TSubclassOf<T>>
{
	enum
	{
		value = TTraitsTemplate<void>::dispatch_value,
	};
	static auto GetName() { return TManualGeneratedName<UObject>::GetName(); }
};
/*
TEnumAsByte<TEnum>
{
 uint8 Value;
}
*/
template<typename T>
struct TTraitsTemplate<TEnumAsByte<T>>
{
	enum
	{
		value = TTraitsTemplate<void>::dispatch_value,
	};
	static auto GetName();
};

// clang-format off
template <typename T, size_t IMetaType = GS_TYPE_TRAITS::TDisjunction<
	  TTraitsEnum<T>
	, THasUObjectClass<T>
	, TTraitsTemplate<T>
	, THasStaticStruct<T>
// 	, TManualGeneratedName<T>
	>::value
>
struct TClass2NameImpl
{
	static const FName& GetName()
	{
		using DT = std::remove_pointer_t<std::decay_t<T>>;
		using Type = std::conditional_t<std::is_base_of<UObject, DT>::value, UObject, DT>;
		static FName name = TManualGeneratedName<Type>::GetName();
		return name;
	}
};
// clang-format on

template<typename T>
auto TTraitsTemplate<TEnumAsByte<T>>::GetName()
{
	return TClass2NameImpl<uint8>::GetName();
}

template<typename InElementType, typename KeyFuncs, typename InAllocator>
auto TTraitsTemplate<TSet<InElementType, KeyFuncs, InAllocator>>::GetName()
{
	using ElementType = InElementType;
	return TTraitsTemplate<void>::GetTSetName(*TClass2NameImpl<ElementType>::GetName().ToString());
}

template<typename InElementType, typename InAllocator>
auto TTraitsTemplate<TArray<InElementType, InAllocator>>::GetName()
{
	using ElementType = InElementType;
	return TTraitsTemplate<void>::GetTArrayName(*TClass2NameImpl<ElementType>::GetName().ToString());
}

template<typename InKeyType, typename InValueType, typename SetAllocator, typename KeyFuncs>
auto TTraitsTemplate<TMap<InKeyType, InValueType, SetAllocator, KeyFuncs>>::GetName()
{
	using KeyType = InKeyType;
	using ValueType = InValueType;
	return TTraitsTemplate<void>::GetTMapName(*TClass2NameImpl<KeyType>::GetName().ToString(), *TClass2NameImpl<ValueType>::GetName().ToString());
}

template<typename T>
struct TClass2NameImpl<T, TManualGeneratedName<void>::dispatch_value>
{
	static const FName& GetName()
	{
		static FName name = TManualGeneratedName<T>::GetName();
		return name;
	}
};

template<typename T>
struct TClass2NameImpl<T, THasUObjectClass<void>::dispatch_value>
{
	static const FName& GetName()
	{
		static FName name = TManualGeneratedName<UObject>::GetName();
		return name;
	}
};

template<typename T>
struct TClass2NameImpl<T, THasStaticStruct<void>::dispatch_value>
{
	static const FName& GetName()
	{
		static FName name = *TBaseStructure<T>::Get()->GetName();
		return name;
	}
};

template<typename T>
struct TClass2NameImpl<T, TTraitsEnum<void>::dispatch_value>
{
	static const FName& GetName()
	{
		static FName name = TManualGeneratedName<typename TTraitsEnum<T>::type>::GetName();
		return name;
	}
};

template<typename T>
struct TClass2NameImpl<T, TTraitsTemplate<void>::dispatch_value>
{
	static const FName& GetName()
	{
		static FName name = TTraitsTemplate<T>::GetName();
		return name;
	}
};
template<typename T>
using TClass2Name = TClass2NameImpl<std::remove_cv_t<std::remove_reference_t<T>>>;
}  // namespace GS_CLASS_TO_NAME
