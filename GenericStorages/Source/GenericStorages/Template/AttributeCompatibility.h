// Copyright UnrealCompatibility, Inc. All Rights Reserved.

#pragma once

#if !defined(ATTRIBUTECOMPATIBILITY_GUARD_H)
#define ATTRIBUTECOMPATIBILITY_GUARD_H

#include "Misc/Attribute.h"
#include "UnrealCompatibility.h"

#if !UE_4_20_OR_LATER
template<typename T, typename SourceType, typename SourceTypeOrBase, typename... PayloadTypes>
FORCEINLINE TAttribute<T> MakeAttributeRaw(SourceType* InObject, T (SourceTypeOrBase::*InMethod)(PayloadTypes...), typename TDecay<PayloadTypes>::Type... InputPayload)
{
	return TAttribute<T>::Create(TAttribute<T>::FGetter::CreateRaw(InObject, InMethod, MoveTemp(InputPayload)...));
}
template<typename T, typename SourceType, typename SourceTypeOrBase, typename... PayloadTypes>
FORCEINLINE TAttribute<T> MakeAttributeRaw(const SourceType* InObject, T (SourceTypeOrBase::*InMethod)(PayloadTypes...) const, typename TDecay<PayloadTypes>::Type... InputPayload)
{
	return TAttribute<T>::Create(TAttribute<T>::FGetter::CreateRaw(InObject, InMethod, MoveTemp(InputPayload)...));
}
template<typename T, typename SourceType, typename SourceTypeOrBase, typename... PayloadTypes>
FORCEINLINE TAttribute<T> MakeAttributeSP(SourceType* InObject, T (SourceTypeOrBase::*InMethod)(PayloadTypes...), typename TDecay<PayloadTypes>::Type... InputPayload)
{
	return TAttribute<T>::Create(TAttribute<T>::FGetter::CreateSP(InObject, InMethod, MoveTemp(InputPayload)...));
}
template<typename T, typename SourceType, typename SourceTypeOrBase, typename... PayloadTypes>
FORCEINLINE TAttribute<T> MakeAttributeSP(const SourceType* InObject, T (SourceTypeOrBase::*InMethod)(PayloadTypes...) const, typename TDecay<PayloadTypes>::Type... InputPayload)
{
	return TAttribute<T>::Create(TAttribute<T>::FGetter::CreateSP(InObject, InMethod, MoveTemp(InputPayload)...));
}
template<typename LambdaType, typename... PayloadTypes>
decltype(auto) MakeAttributeLambda(LambdaType&& InCallable, PayloadTypes&&... InputPayload)
{
	typedef decltype(InCallable(DeclVal<PayloadTypes>()...)) T;

	return TAttribute<T>::Create(TAttribute<T>::FGetter::CreateLambda(InCallable, Forward<PayloadTypes>(InputPayload)...));
}
#endif

template<typename UserClass, ESPMode SPMode, typename LambdaType, typename... PayloadTypes>
inline auto MakeAttributeWeakLambda(const TSharedRef<UserClass, SPMode>& InUserObject, LambdaType&& InCallable, PayloadTypes&&... InputPayload)
{
	using T = decltype(InCallable(DeclVal<PayloadTypes>()...));
	return TAttribute<T>::Create(CreateSPLambda(InUserObject, Forward<LambdaType>(InCallable)), Forward<PayloadTypes>(InputPayload)...);
}

template<typename UserObject, typename LambdaType, typename... PayloadTypes>
inline auto MakeAttributeWeakLambda(const UserObject* InUserObject, LambdaType&& InCallable, PayloadTypes&&... InputPayload)
{
	using T = decltype(InCallable(DeclVal<PayloadTypes>()...));
	return TAttribute<T>::Create(CreateWeakLambda(InUserObject, Forward<LambdaType>(InCallable)), Forward<PayloadTypes>(InputPayload)...);
}

#endif  // !defined(ATTRIBUTECOMPATIBILITY_GUARD_H)
