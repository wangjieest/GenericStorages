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
#include "GS_Compatibility.h"

#include <tuple>

#if !defined(GS_TYPE_TRAITS)
#	define GS_TYPE_TRAITS GS_TypeTraits
#endif

namespace GS_TYPE_TRAITS
{
// is_detected
template<typename, template<typename...> class Op, typename... T>
struct IsDetectedImpl : std::false_type
{
};
template<template<typename...> class Op, typename... T>
struct IsDetectedImpl<VoidType<Op<T...>>, Op, T...> : std::true_type
{
};
template<template<typename...> class Op, typename... T>
using IsDetected = IsDetectedImpl<void, Op, T...>;

template<typename... Ts>
struct TDisjunction;
template<typename V, typename... Is>
struct TDisjunction<V, Is...>
{
	enum
	{
		value = V::value ? V::value : TDisjunction<Is...>::value
	};
};
template<>
struct TDisjunction<>
{
	enum
	{
		value = 0
	};
};

struct UnUseType
{
};

template<typename T>
struct TIsCallable
{
private:
	template<typename V>
	using IsCallableType = decltype(&V::operator());
	template<typename V>
	using IsCallable = IsDetected<IsCallableType, V>;

public:
	static const bool value = IsCallable<T>::value;
};

template<typename T>
struct TIsBaseDelegate
{
private:
	template<typename V>
	using IsBaseDelegateType = decltype(&V::ExecuteIfBound);
	template<typename V>
	using IsBaseDelegate = IsDetected<IsBaseDelegateType, V>;

public:
	static const bool value = IsBaseDelegate<T>::value;
};

template<typename... Args>
struct TGetFirst;
template<>
struct TGetFirst<>
{
	using type = UnUseType;
};
template<typename T, typename... Args>
struct TGetFirst<T, Args...>
{
	using type = T;
};

template<typename... Args>
using TGetFirstType = typename TGetFirst<Args...>::type;

template<typename T, typename... Args>
struct TIsFirstSame
{
	using FirstType = TGetFirstType<Args...>;
	enum
	{
		value = TIsSame<typename TRemoveCV<FirstType>::Type, T>::Value
	};
};
template<typename T>
struct TIsFirstSame<T>
{
	enum
	{
		value = 0
	};
};

template<typename... Args>
struct TIsFirstTypeCallable;
template<>
struct TIsFirstTypeCallable<>
{
	using type = std::false_type;
	enum
	{
		value = type::value
	};
};
template<typename T, typename... Args>
struct TIsFirstTypeCallable<T, Args...>
{
	using type = std::conditional_t<TIsCallable<T>::value, std::true_type, std::false_type>;
	enum
	{
		value = type::value
	};
};

template<typename... Args>
struct TGetLast;
template<>
struct TGetLast<>
{
	using type = UnUseType;
};
template<typename T, typename... Args>
struct TGetLast<T, Args...>
{
	using type = std::tuple_element_t<sizeof...(Args), std::tuple<T, Args...>>;
};

template<typename... Args>
using TGetLastType = typename TGetLast<Args...>::type;

template<typename T, typename... Args>
struct TIsLastSame
{
	using LastType = TGetLastType<Args...>;
	enum
	{
		value = TIsSame<typename TRemoveCV<LastType>::Type, T>::Value
	};
};
template<typename T>
struct TIsLastSame<T>
{
	enum
	{
		value = 0
	};
};

template<typename... Args>
using TGetLastType = typename TGetLast<Args...>::type;

template<typename... Args>
struct TIsLastTypeCallable;
template<>
struct TIsLastTypeCallable<>
{
	enum
	{
		callable_type = 0,
		delegate_type = 0,
		value = 0
	};
	using type = std::false_type;
};
template<typename C, typename... Args>
struct TIsLastTypeCallable<C, Args...>
{
	using LastType = std::tuple_element_t<sizeof...(Args), std::tuple<C, Args...>>;
	enum
	{
		callable_type = TIsCallable<LastType>::value,
		delegate_type = TIsBaseDelegate<LastType>::value,
		value = callable_type || delegate_type
	};
	using type = std::conditional_t<value, std::true_type, std::false_type>;
};

template<typename Tuple>
struct TTupleRemoveLast;

template<>
struct TTupleRemoveLast<std::tuple<>>
{
	using type = std::tuple<>;
};

template<typename T, typename... Args>
struct TTupleRemoveLast<std::tuple<T, Args...>>
{
	using Tuple = std::tuple<T, Args...>;
	template<std::size_t... Is>
	static auto extract(std::index_sequence<Is...>) -> std::tuple<std::tuple_element_t<Is, Tuple>...>;
	using type = decltype(extract(std::make_index_sequence<std::tuple_size<Tuple>::value - 1>()));
};
template<typename Tuple>
using TTupleRemoveLastType = typename TTupleRemoveLast<std::remove_cv_t<std::remove_reference_t<Tuple>>>::type;

template<typename... Args>
struct TupleTypeRemoveLast;

template<>
struct TupleTypeRemoveLast<>
{
	using Tuple = std::tuple<>;
	using type = void;
};

template<typename T, typename... Args>
struct TupleTypeRemoveLast<T, Args...>
{
	using Tuple = std::tuple<T, Args...>;
	using type = TTupleRemoveLastType<Tuple>;
};

template<typename... Args>
using TupleTypeRemoveLastType = typename TupleTypeRemoveLast<Args...>::type;

template<typename E>
constexpr std::underlying_type_t<E> ToUnderlying(E e)
{
	return static_cast<std::underlying_type_t<E>>(e);
}

template<typename TValue, typename TAddr>
union HorribleUnion
{
	TValue Value;
	TAddr Addr;
};
template<typename TValue, typename TAddr>
FORCEINLINE TValue HorribleFromAddr(TAddr Addr)
{
	HorribleUnion<TValue, TAddr> u;
	static_assert(sizeof(TValue) >= sizeof(TAddr), "Cannot use horrible_cast<>");
	u.Value = 0;
	u.Addr = Addr;
	return u.Value;
}
template<typename TAddr, typename TValue>
FORCEINLINE TAddr HorribleToAddr(TValue Value)
{
	HorribleUnion<TValue, TAddr> u;
	static_assert(sizeof(TValue) >= sizeof(TAddr), "Cannot use horrible_cast<>");
	u.Value = Value;
	return u.Addr;
}

template<typename T>
struct StructUtil
{
	using RawType = std::decay_t<std::remove_cv_t<std::remove_reference_t<T>>>;
	static class UStruct* GetStruct() { RawType::StaticStruct(); }
};
// template<size_t Idx, typename Tup>
// CONSTEXPR size_t TupleOffset()
// {
// 	return static_cast<size_t>(reinterpret_cast<char*>(&std::get<Idx>(*reinterpret_cast<Tup*>(0))) -
// 							   reinterpret_cast<char*>(0));
// }
}  // namespace GS_TYPE_TRAITS
