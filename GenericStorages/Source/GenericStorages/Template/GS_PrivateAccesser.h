// Copyright 2018-2020 wangjieest, Inc. All Rights Reserved.

#pragma once

#include <type_traits>
#include <utility>

#define GS_PRIVATEACCESSER_MEMBER(Class, Member, ...)                                           \
	namespace PrivateAccess                                                                     \
	{                                                                                           \
	using MemberPtr##Class##Member##Type = __VA_ARGS__;                                         \
	using MemberPtr##Class##Member = MemberPtr##Class##Member##Type Class::*;                   \
	template<MemberPtr##Class##Member MemPtr>                                                   \
	struct Get##Class##Member                                                                   \
	{                                                                                           \
		friend MemberPtr##Class##Member Access##Class##Member() { return MemPtr; }              \
	};                                                                                          \
	MemberPtr##Class##Member Access##Class##Member();                                           \
	template struct Get##Class##Member<&##Class::##Member>;                                     \
	auto& Member(const Class& obj) { return const_cast<Class&>(obj).*Access##Class##Member(); } \
	}

#define GS_PRIVATEACCESSER_FUNCTION(Class, FuncName, ...)                                                                           \
	namespace PrivateAccess                                                                                                         \
	{                                                                                                                               \
	using FuncPtr##Class##FuncName##Type = __VA_ARGS__;                                                                             \
	using FuncPtr##Class##FuncName = FuncPtr##Class##FuncName##Type Class::*;                                                       \
	template<FuncPtr##Class##FuncName MemPtr>                                                                                       \
	struct Get##Class##FuncName                                                                                                     \
	{                                                                                                                               \
		friend FuncPtr##Class##FuncName Access##Class##FuncName() { return MemPtr; }                                                \
	};                                                                                                                              \
	FuncPtr##Class##FuncName Access##Class##FuncName();                                                                             \
	template struct Get##Class##FuncName<&##Class::##FuncName>;                                                                     \
	template<typename Obj, std::enable_if_t<std::is_same<std::remove_reference_t<Obj>, Class>::value>* = nullptr, typename... Args> \
	decltype(auto) FuncName(Obj&& obj, Args&&... args)                                                                              \
	{                                                                                                                               \
		return (std::forward<Obj>(obj).*Access##Class##FuncName())(std::forward<Args>(args)...);                                    \
	}                                                                                                                               \
	}

// see @https://github.com/martong/access_private/blob/master/include/access_private.hpp
namespace DetailPrivateAccess
{
template<typename PtrType, PtrType PtrValue, typename TagType>
struct private_access
{
	friend PtrType get(TagType) { return PtrValue; }
};
}  // namespace DetailPrivateAccess

#define PRIVATE_ACCESS_DETAIL_CONCATENATE_IMPL(x, y) x##y
#define PRIVATE_ACCESS_DETAIL_CONCATENATE(x, y) PRIVATE_ACCESS_DETAIL_CONCATENATE_IMPL(x, y)

#define PRIVATE_ACCESS_DETAIL_ACCESS_PRIVATE(Tag, Class, Name, PtrTypeKind, ...)                                             \
	namespace DetailPrivateAccess                                                                                            \
	{                                                                                                                        \
		struct Tag                                                                                                           \
		{                                                                                                                    \
		};                                                                                                                   \
		template struct private_access<decltype(&Class::Name), &Class::Name, Tag>;                                           \
		using PRIVATE_ACCESS_DETAIL_CONCATENATE(Alias_, Tag) = __VA_ARGS__;                                                  \
		using PRIVATE_ACCESS_DETAIL_CONCATENATE(PtrType_, Tag) = PRIVATE_ACCESS_DETAIL_CONCATENATE(Alias_, Tag) PtrTypeKind; \
		PRIVATE_ACCESS_DETAIL_CONCATENATE(PtrType_, Tag) get(Tag);                                                           \
	}

#define PRIVATE_ACCESS_DETAIL_ACCESS_PRIVATE_FIELD(Tag, Class, Name, ...)                                                   \
	PRIVATE_ACCESS_DETAIL_ACCESS_PRIVATE(Tag, Class, Name, Class::*, __VA_ARGS__)                                           \
	namespace                                                                                                               \
	{                                                                                                                       \
		namespace PrivateAccess                                                                                             \
		{                                                                                                                   \
			auto& Name(Class&& t) { return t.*get(DetailPrivateAccess::Tag{}); }                                            \
			auto& Name(Class& t) { return t.*get(DetailPrivateAccess::Tag{}); }                                             \
			using PRIVATE_ACCESS_DETAIL_CONCATENATE(X, Tag) = __VA_ARGS__;                                                  \
			using PRIVATE_ACCESS_DETAIL_CONCATENATE(Y, Tag) = const PRIVATE_ACCESS_DETAIL_CONCATENATE(X, Tag);              \
			PRIVATE_ACCESS_DETAIL_CONCATENATE(Y, Tag) & Name(const Class& t) { return t.*get(DetailPrivateAccess::Tag{}); } \
		}                                                                                                                   \
	}

#define PRIVATE_ACCESS_DETAIL_ACCESS_PRIVATE_FUN(Tag, Class, Name, ...)                                                                     \
	PRIVATE_ACCESS_DETAIL_ACCESS_PRIVATE(Tag, Class, Name, Class::*, __VA_ARGS__)                                                           \
	namespace                                                                                                                               \
	{                                                                                                                                       \
		namespace PrivateAccess                                                                                                             \
		{                                                                                                                                   \
			template<typename Obj, std::enable_if_t<std::is_same<std::remove_reference_t<Obj>, Class>::value>* = nullptr, typename... Args> \
			decltype(auto) Name(Obj&& o, Args&&... args)                                                                                    \
			{                                                                                                                               \
				return (std::forward<Obj>(o).*get(DetailPrivateAccess::Tag{}))(std::forward<Args>(args)...);                                \
			}                                                                                                                               \
		}                                                                                                                                   \
	}

#define PRIVATE_ACCESS_DETAIL_ACCESS_PRIVATE_STATIC_FIELD(Tag, Class, Name, ...) \
	PRIVATE_ACCESS_DETAIL_ACCESS_PRIVATE(Tag, Class, Name, *, __VA_ARGS__)       \
	namespace                                                                    \
	{                                                                            \
		namespace PrivateAccessStatic                                            \
		{                                                                        \
			namespace Class                                                      \
			{                                                                    \
				Type& Name() { return *get(DetailPrivateAccess::Tag{}); }        \
			}                                                                    \
		}                                                                        \
	}

#define PRIVATE_ACCESS_DETAIL_ACCESS_PRIVATE_STATIC_FUN(Tag, Class, Name, ...)           \
	PRIVATE_ACCESS_DETAIL_ACCESS_PRIVATE(Tag, Class, Name, *, __VA_ARGS__)               \
	namespace                                                                            \
	{                                                                                    \
		namespace PrivateAccessStatic                                                    \
		{                                                                                \
			namespace Class                                                              \
			{                                                                            \
				template<typename... Args>                                               \
				decltype(auto) Name(Args&&... args)                                      \
				{                                                                        \
					return get(DetailPrivateAccess::Tag{})(std::forward<Args>(args)...); \
				}                                                                        \
			}                                                                            \
		}                                                                                \
	}

#define PRIVATE_ACCESS_DETAIL_UNIQUE_TAG PRIVATE_ACCESS_DETAIL_CONCATENATE(PrivateAccessTag, __COUNTER__)
#define ACCESS_PRIVATE_FIELD(Class, Name, ...) PRIVATE_ACCESS_DETAIL_ACCESS_PRIVATE_FIELD(PRIVATE_ACCESS_DETAIL_UNIQUE_TAG, Class, Name, __VA_ARGS__)
#define ACCESS_PRIVATE_FUN(Class, Name, ...) PRIVATE_ACCESS_DETAIL_ACCESS_PRIVATE_FUN(PRIVATE_ACCESS_DETAIL_UNIQUE_TAG, Class, Name, __VA_ARGS__)
#define ACCESS_PRIVATE_STATIC_FIELD(Class, Name, ...) PRIVATE_ACCESS_DETAIL_ACCESS_PRIVATE_STATIC_FIELD(PRIVATE_ACCESS_DETAIL_UNIQUE_TAG, Class, Name, __VA_ARGS__)
#define ACCESS_PRIVATE_STATIC_FUN(Class, Name, ...) PRIVATE_ACCESS_DETAIL_ACCESS_PRIVATE_STATIC_FUN(PRIVATE_ACCESS_DETAIL_UNIQUE_TAG, Class, Name, __VA_ARGS__)
