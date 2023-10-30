// Copyright GenericStorages, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

#include "MACRO_FOR_EACH.h"

#include <type_traits>

namespace transcribe
{
template<class Src, class Dst>
using const_t = std::conditional_t<std::is_const<Src>::value, Dst const, Dst>;
template<class Src, class Dst>
using volatile_t = std::conditional_t<std::is_volatile<Src>::value, Dst volatile, Dst>;
template<class Src, class Dst>
using point_t = std::conditional_t<std::is_pointer<Src>::value, Dst*, Dst>;
template<class Src, class Dst>
using refernece_t = std::conditional_t<std::is_reference<Src>::value, Dst*, Dst>;
template<class Src, class Dst>
using cv_t = const_t<Src, volatile_t<Src, Dst>>;
template<class Src, class Dst>
using cvp_t = cv_t<Src, point_t<Src, Dst>>;
template<class Src, class Dst>
using cvrp_t = refernece_t<Src, cvp_t<Src, Dst>>;

template<typename T>
T* ToRawPtr(T* Ptr)
{
	return Ptr;
}
template<typename T>
auto ToRawPtr(const TSharedPtr<T>& Ptr)
{
	return Ptr.Get();
}
template<typename T>
auto ToRawPtr(const TSharedRef<T>& Ptr)
{
	return &Ptr.Get();
}
}  // namespace transcribe

#define Z_GS_USING_MEMBER_(Member) using Z_SuperType::Member
#define Z_GS_USING_MEMBER(...) MACRO_FOR_EACH(Z_GS_USING_MEMBER_, ;, ##__VA_ARGS__)

#define GS_DECLARE_PROTECT(Type, ...)   \
	struct Type##Friend : public Type   \
	{                                   \
		using Z_SuperType = Type;       \
		Z_GS_USING_MEMBER(__VA_ARGS__); \
	};

#define GS_ACCESS_PROTECT(Inst, Type, ...)                                   \
	[](auto C) {                                                             \
		using Z_InstType = decltype(C);                                      \
		using Z_DecayType = std::decay_t<std::remove_pointer_t<Z_InstType>>; \
		static_assert(std::is_base_of<Type, Z_DecayType>::value, "err");     \
		struct Type##Friend : public Type                                    \
		{                                                                    \
			using Z_SuperType = std::common_type_t<Z_DecayType, Type>;       \
			Z_GS_USING_MEMBER(__VA_ARGS__);                                  \
		};                                                                   \
		return static_cast<transcribe::cvrp_t<Z_InstType, Type##Friend>>(C); \
	}(transcribe::ToRawPtr(Inst))
