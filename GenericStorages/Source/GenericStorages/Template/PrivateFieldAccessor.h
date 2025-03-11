// Copyright GenericStorages, Inc. All Rights Reserved.

#pragma once

#include <type_traits>
#include <utility>

#define GS_PRIVATEACCESS_MEMBER(Class, Member, ...)                                             \
	namespace PrivateAccess                                                                     \
	{                                                                                           \
	using Z_##MemberPtr##Class##Member##M##Type = __VA_ARGS__;                                  \
	using Z_##MemberPtr##Class##Member = Z_##MemberPtr##Class##Member##M##Type Class::*;        \
	template<Z_##MemberPtr##Class##Member MemPtr>                                               \
	struct Z_Get##Class##Member                                                                 \
	{                                                                                           \
		friend Z_##MemberPtr##Class##Member Access##Class##Member() { return MemPtr; }          \
	};                                                                                          \
	Z_##MemberPtr##Class##Member Access##Class##Member();                                       \
	template struct Z_Get##Class##Member<&Class::Member>;                                       \
	auto& Member(const Class& Ref) { return const_cast<Class&>(Ref).*Access##Class##Member(); } \
	}

#define GS_PRIVATEACCESS_FUNCTION(Class, FuncName, ...)                                                                         \
	namespace PrivateAccess                                                                                                     \
	{                                                                                                                           \
	using Z_##FuncPtr##Class##FuncName##F##Type = __VA_ARGS__;                                                                  \
	using Z_##FuncPtr##Class##FuncName = Z_##FuncPtr##Class##FuncName##F##Type Class::*;                                        \
	template<Z_##FuncPtr##Class##FuncName MemPtr>                                                                               \
	struct Z_Get##Class##FuncName                                                                                               \
	{                                                                                                                           \
		friend Z_##FuncPtr##Class##FuncName Access##Class##FuncName() { return MemPtr; }                                        \
	};                                                                                                                          \
	Z_##FuncPtr##Class##FuncName Access##Class##FuncName();                                                                     \
	template struct Z_Get##Class##FuncName<&Class::FuncName>;                                                                   \
	template<typename T, std::enable_if_t<std::is_same<std::remove_reference_t<T>, Class>::value>* = nullptr, typename... Args> \
	decltype(auto) FuncName(T&& Ref, Args&&... args)                                                                            \
	{                                                                                                                           \
		return (std::forward<T>(Ref).*Access##Class##FuncName())(std::forward<Args>(args)...);                                  \
	}                                                                                                                           \
	}

#define GS_PRIVATEACCESS_STATIC_MEMBER(Class, Member, ...)                              \
	namespace PrivateAccessStatic                                                       \
	{                                                                                   \
	using ZZ_##MemberPtr##Class##Member##Type = __VA_ARGS__;                            \
	using ZZ_##MemberPtr##Class##Member = ZZ_##MemberPtr##Class##Member##Type*;         \
	template<ZZ_##MemberPtr##Class##Member MemPtr>                                      \
	struct ZZ_Get##Class##Member                                                        \
	{                                                                                   \
		friend ZZ_##MemberPtr##Class##Member Access##Class##Member() { return MemPtr; } \
	};                                                                                  \
	ZZ_##MemberPtr##Class##Member Access##Class##Member();                              \
	template struct ZZ_Get##Class##Member<&Class::Member>;                              \
	auto& Member() { return *Access##Class##Member(); }                                 \
	}

#define GS_PRIVATEACCESS_STATIC_FUNCTION(Class, FuncName, ...)                            \
	namespace PrivateAccessStatic                                                         \
	{                                                                                     \
	using ZZ_##FuncPtr##Class##FuncName##Type = __VA_ARGS__;                              \
	using ZZ_##FuncPtr##Class##FuncName = ZZ_##FuncPtr##Class##FuncName##Type*;           \
	template<ZZ_##FuncPtr##Class##FuncName MemPtr>                                        \
	struct ZZ_Get##Class##FuncName                                                        \
	{                                                                                     \
		friend ZZ_##FuncPtr##Class##FuncName Access##Class##FuncName() { return MemPtr; } \
	};                                                                                    \
	ZZ_##FuncPtr##Class##FuncName Access##Class##FuncName();                              \
	template struct ZZ_Get##Class##FuncName<&Class::FuncName>;                            \
	template<typename... Args>                                                            \
	decltype(auto) FuncName(Args&&... args)                                               \
	{                                                                                     \
		return (*Access##Class##FuncName())(std::forward<Args>(args)...);                 \
	}                                                                                     \
	}

#define GS_PRIVATEACCESS_FUNCTION_NAME(Class, FuncName, ...) \
	GS_PRIVATEACCESS_FUNCTION(Class, FuncName, __VA_ARGS__)  \
	namespace PrivateAccess                                  \
	{                                                        \
	namespace Class                                          \
	{                                                        \
		constexpr auto FuncName = TEXT(#FuncName);           \
	}                                                        \
	}
