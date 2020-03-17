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

#ifndef UNREAL_COMPATIBILITY_GUARD_H
#	define UNREAL_COMPATIBILITY_GUARD_H

#	include "Launch/Resources/Version.h"
#	include "UObject/UnrealType.h"

#	include <type_traits>

// void_t
template<typename... T>
struct MakeVoid
{
	using type = void;
};
template<typename... T>
using VoidType = typename MakeVoid<T...>::type;

// Init Once
template<typename Type>
bool TrueOnFirstCall(const Type&)
{
	static bool bValue = true;
	bool Result = bValue;
	bValue = false;
	return Result;
}

#	if ENGINE_MINOR_VERSION <= 20
namespace ITS
{
	template<typename E>
	using is_scoped_enum = std::integral_constant<bool, std::is_enum<E>::value && !std::is_convertible<E, int>::value>;
	/*
	GCC:
	void foo() [with T = {type}]
	clang:
	void foo() [T = {type}]
	MSVC:
	void __cdecl foo<{type}>(void)
	*/
#		if defined(_MSC_VER)
#			define Z_TEMPLATE_PARAMETER_NAME_ __FUNCSIG__
	constexpr unsigned int FRONT_SIZE = sizeof("static const char* __cdecl ITS::TypeStr<") - 1u;
	constexpr unsigned int BACK_SIZE = sizeof(">(void)") - 1u;
#		else
#			define Z_TEMPLATE_PARAMETER_NAME_ __PRETTY_FUNCTION__
#			if defined(__clang__)
	constexpr unsigned int FRONT_SIZE = sizeof("static const char* ITS::TypeStr() [T = ") - 1u;
	constexpr unsigned int BACK_SIZE = sizeof("]") - 1u;
#			else
	constexpr unsigned int FRONT_SIZE = sizeof("static const char* ITS::TypeStr() [with T = ") - 1u;
	constexpr unsigned int BACK_SIZE = sizeof("]") - 1u;
#			endif
#		endif

	template<typename EnumType>
	static const char* TypeStr(void)
	{
		static_assert(std::is_enum<EnumType>::value, "err");
		constexpr int32 size = sizeof(Z_TEMPLATE_PARAMETER_NAME_) - FRONT_SIZE - BACK_SIZE;
		static char typeName[size] = {};
		memcpy(typeName, Z_TEMPLATE_PARAMETER_NAME_ + FRONT_SIZE, size - 1u);

		return typeName;
	}
}  // namespace ITS

template<typename EnumType>
UEnum* StaticEnum()
{
	static UEnum* Ret = ::FindObject<UEnum>(ANY_PACKAGE, ANSI_TO_TCHAR(ITS::TypeStr<EnumType>()), true);
	return Ret;
}
template<typename ClassType>
UClass* StaticClass()
{
	return ClassType::StaticClass();
}
template<typename StructType>
UScriptStruct* StaticStruct()
{
	return TBaseStructure<StructType>::Get();
}
#	else
#		include "UObject/ReflectedTypeAccessors.h"
#	endif

using FUnsizedIntProperty = UE4Types_Private::TIntegerPropertyMapping<signed int>::Type;
using FUnsizedUIntProperty = UE4Types_Private::TIntegerPropertyMapping<unsigned int>::Type;

#	if ENGINE_MINOR_VERSION < 25
using FFieldClass = UClass;
using FField = UField;

using FProperty = UProperty;
#		define CASTCLASS_FProperty CASTCLASS_UProperty

using FBoolProperty = UBoolProperty;
#		define CASTCLASS_FBoolProperty CASTCLASS_UBoolProperty

#		define CASTCLASS_FNumericProperty CASTCLASS_UNumericProperty
using FInt8Property = UInt8Property;
#		define CASTCLASS_FInt8Property CASTCLASS_UInt8Property
using FByteProperty = UByteProperty;
#		define CASTCLASS_FByteProperty CASTCLASS_UByteProperty

using FInt16Property = UInt16Property;
#		define CASTCLASS_FInt16Property CASTCLASS_UInt16Property
using FUInt16Property = UUInt16Property;
#		define CASTCLASS_FUInt16Property CASTCLASS_UUInt16Property

using FIntProperty = UIntProperty;
#		define CASTCLASS_FIntProperty CASTCLASS_UIntProperty
using FUInt32Property = UUInt32Property;
#		define CASTCLASS_FUInt32Property CASTCLASS_UUInt32Property

using FInt64Property = UInt64Property;
#		define CASTCLASS_FInt64Property CASTCLASS_UInt64Property
using FUInt64Property = UUInt64Property;
#		define CASTCLASS_FUInt64Property CASTCLASS_UUInt64Property

using FEnumProperty = UEnumProperty;
#		define CASTCLASS_FEnumProperty CASTCLASS_UEnumProperty

using FFloatProperty = UFloatProperty;
#		define CASTCLASS_FFloatProperty CASTCLASS_UFloatProperty
using FDoubleProperty = UDoubleProperty;
#		define CASTCLASS_FDoubleProperty CASTCLASS_UDoubleProperty

using FStructProperty = UStructProperty;
#		define CASTCLASS_FStructProperty CASTCLASS_UStructProperty

using FStrProperty = UStrProperty;
#		define CASTCLASS_FStrProperty CASTCLASS_UStrProperty
using FNameProperty = UNameProperty;
#		define CASTCLASS_FNameProperty CASTCLASS_UNameProperty
using FTextProperty = UTextProperty;
#		define CASTCLASS_FTextProperty CASTCLASS_UTextProperty

using FArrayProperty = UArrayProperty;
#		define CASTCLASS_FArrayProperty CASTCLASS_UArrayProperty
using FMapProperty = UMapProperty;
#		define CASTCLASS_FMapProperty CASTCLASS_UMapProperty
using FSetProperty = USetProperty;
#		define CASTCLASS_FSetProperty CASTCLASS_USetProperty

using FObjectPropertyBase = UObjectPropertyBase;
#		define CASTCLASS_FObjectPropertyBase CASTCLASS_UObjectPropertyBase
using FObjectProperty = UObjectProperty;
#		define CASTCLASS_FObjectProperty CASTCLASS_UObjectProperty
using FClassProperty = UClassProperty;
#		define CASTCLASS_FClassProperty CASTCLASS_UClassProperty
using FWeakObjectProperty = UWeakObjectProperty;
#		define CASTCLASS_FWeakObjectProperty CASTCLASS_UWeakObjectProperty
using FSoftObjectProperty = USoftObjectProperty;
#		define CASTCLASS_FSoftObjectProperty CASTCLASS_USoftObjectProperty
using FSoftClassProperty = USoftClassProperty;
#		define CASTCLASS_FSoftClassProperty CASTCLASS_USoftClassProperty
using FLazyObjectProperty = ULazyObjectProperty;
#		define CASTCLASS_FLazyObjectProperty CASTCLASS_ULazyObjectProperty
using FInterfaceProperty = UInterfaceProperty;
#		define CASTCLASS_FInterfaceProperty CASTCLASS_UInterfaceProperty

using FDelegateProperty = UDelegateProperty;
#		define CASTCLASS_FDelegateProperty CASTCLASS_UDelegateProperty
using FMulticastDelegateProperty = UMulticastDelegateProperty;
#		define CASTCLASS_FMulticastDelegateProperty CASTCLASS_UMulticastDelegateProperty

#		define CASTCLASS_FMulticastInlineDelegateProperty CASTCLASS_UMulticastInlineDelegateProperty
#		define CASTCLASS_FMulticastSparseDelegateProperty CASTCLASS_UMulticastSparseDelegateProperty

//////////////////////////////////////////////////////////////////////////

template<typename To, typename From>
FORCEINLINE auto CastField(From* Prop)
{
	return Cast<To>(Prop);
}
template<typename To, typename From>
FORCEINLINE auto CastFieldChecked(From* Prop)
{
	return CastChecked<To>(Prop);
}

FORCEINLINE EClassCastFlags GetPropCastFlags(FProperty* Prop)
{
	return (EClassCastFlags)Prop->GetClass()->ClassCastFlags;
}

template<typename T>
UClass* GetFieldOwnerClass(T* Field)
{
	return CastChecked<UClass>(Field->GetOuter());
}

template<typename T>
struct TFieldPath
{
public:
	TFieldPath(T* InField = nullptr)
		: Field(InField)
	{
	}

	void operator=(T* InField) { Field = InField; }

	T* operator*() const { return Field; }
	T* operator->() const { return Field; }

	explicit bool operator bool() const { return !!Field; }
	T* Get() const { return Field; }
	T& GetOwnerVariant() const { return *Field; }
	UClass* GetOwnerClass() const { return GetFieldOwnerClass(Field); }

protected:
	mutable T* Field;
};

template<typename T>
using TFieldPathType = T*;
template<typename T>
FORCEINLINE T* GetPropPtr(T* Field)
{
	return Field;
}
template<typename T>
FORCEINLINE T* GetPropPtr(TFieldPath<T>& Field)
{
	return Field.Get();
}

FORCEINLINE UObject* GetOwnerUObject(FProperty* Prop)
{
	return Prop->GetOuter();
}
FORCEINLINE const FString& GetFieldName(FProperty* Prop)
{
	return Prop->GetOwner()->GetName();
}

#	else  // not (ENGINE_MINOR_VERSION < 25)

//////////////////////////////////////////////////////////////////////////

#		include "UObject/FieldPath.h"
FORCEINLINE EClassCastFlags GetPropCastFlags(FProperty* Prop)
{
	return (EClassCastFlags)Prop->GetCastFlags();
}
template<typename T>
using TFieldPathType = TFieldPath<T>;

template<typename T>
FORCEINLINE UClass* GetFieldOwnerClass(T* Field)
{
	return Field->GetOwnerClass();
}
template<typename T>
FORCEINLINE T* GetPropPtr(T* Field)
{
	return Field;
}
template<typename T>
FORCEINLINE FProperty* GetPropPtr(const TFieldPath<T>& Field)
{
	return const_cast<TFieldPath<T>&>(Field).Get();
}
FORCEINLINE UObject* GetOwnerUObject(FProperty* Prop)
{
	return Prop->GetOwner<UObject>();
}
FORCEINLINE const FString& GetFieldName(FProperty* Prop)
{
	return Prop->GetOwnerVariant().GetName();
}
#	endif
#endif
