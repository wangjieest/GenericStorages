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
#include "CoreUObject.h"

#include "Logging/LogMacros.h"
#include "Misc/AssertionMacros.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/WeakObjectPtr.h"
#include "GS_Compatibility.h"

#if ENGINE_MINOR_VERSION >= 23
#	include "Containers/LockFreeList.h"
#endif

#include <memory>

#if !defined(GS_SIGNALS)
#	define GS_SIGNALS GS_Signals
#endif

namespace GS_SIGNALS
{
class FSignalsHandle;
using KeyType = uint64;
namespace Details
{
	// clang-format off
	template<typename... Args> 
	using TSlot = TFunction<void(Args...)>;
#if ENGINE_MINOR_VERSION >= 21
	template<typename... Args>
	using TUniqueSlot = TUniqueFunction<void(Args...)>;
#else
	template<typename... Args>
	using TUniqueSlot = TSlot<Args...>;
#endif
	template<typename T>
	using TestSignalsHandle = std::bool_constant<std::is_base_of<FSignalsHandle, std::remove_cv_t<std::remove_reference_t<T>>>::value>;
	template<typename T>
	using ForwardParam = typename std::conditional<std::is_reference<T>::value || std::is_pointer<T>::value || std::is_arithmetic<T>::value, T, T&&>::type;

	template<typename... FuncArgs>
	struct Invoker
	{
	private:
		template<typename F, typename... Args>
		FORCEINLINE static decltype(auto) ApplyImpl(const F& f, Args&&... args)
		{
			auto ftor = [&](ForwardParam<FuncArgs>... iargs, auto&&...) { return f(static_cast<FuncArgs>(iargs)...); };
			return ftor(std::forward<Args>(args)...);
		}
		template<typename T, typename R, typename... Args>
		FORCEINLINE static decltype(auto) ApplyImpl(R (T::*f)(FuncArgs...), T* const Obj, Args&&... args)
		{
			auto ftor = [&](ForwardParam<FuncArgs>... iargs, auto&&...) { return (Obj->*f)(static_cast<FuncArgs>(iargs)...); };
			return ftor(std::forward<Args>(args)...);
		}
		template<typename T, typename R, typename... Args>
		FORCEINLINE static decltype(auto) ApplyImpl(R (T::*f)(FuncArgs...) const, T const* const Obj, Args&&... args)
		{
			auto ftor = [&](ForwardParam<FuncArgs>... iargs, auto&&...) { return (Obj->*f)(static_cast<FuncArgs>(iargs)...); };
			return ftor(std::forward<Args>(args)...);
		}

#if defined(_MSC_VER) && !defined(__clang__)  // replace auto&&... with ... in the lambda
		template<size_t Cnt, size_t I, typename T>
		static FORCEINLINE decltype(auto) Conv(T&& t, typename std::enable_if<(I < Cnt)>::type* tag = nullptr) { return std::forward<T>(t); }
		template<size_t Cnt, size_t I, typename T>
		static FORCEINLINE const void* Conv(T&& t, typename std::enable_if<(I >= Cnt)>::type* tag = nullptr) { return nullptr; }

		template<size_t... Is, typename F, typename... Args>
		FORCEINLINE static decltype(auto) ApplyImpl2(std::index_sequence<Is...>*, const F& f, Args&&... args)
		{
			auto ftor = [&](ForwardParam<FuncArgs>... iargs, ...) { return f(static_cast<FuncArgs>(iargs)...); };
			return ftor(Conv<sizeof(... FuncArgs), Is, Args>(args)...);
		}
		template<size_t... Is, typename T, typename R, typename... Args>
		FORCEINLINE static decltype(auto) ApplyImpl2(std::index_sequence<Is...>*, R (T::*f)(FuncArgs...), T* const Obj, Args&&... args)
		{
			auto ftor = [&](ForwardParam<FuncArgs>... iargs, ...) { return (Obj->*f)(static_cast<FuncArgs>(iargs)...); };
			return ftor(Conv<sizeof(... FuncArgs), Is, Args>(args)...);
		}
		template<size_t... Is, typename T, typename R, typename... Args>
		FORCEINLINE static decltype(auto) ApplyImpl2(std::index_sequence<Is...>*, R (T::*f)(FuncArgs...) const, T const* const Obj, Args&&... args)
		{
			auto ftor = [&](ForwardParam<FuncArgs>... iargs, ...) { return (Obj->*f)(static_cast<FuncArgs>(iargs)...); };
			return ftor(Conv<sizeof(... FuncArgs), Is, Args>(args)...);
		}
		template<typename F, typename... Args>
		FORCEINLINE static decltype(auto) ProxyApply(std::true_type, const F& f, Args&&... args) { return ApplyImpl(f, std::forward<Args>(args)...); }
		template<typename F, typename... Args>
		FORCEINLINE static decltype(auto) ProxyApply(std::false_type, const F& f, Args&&... args) { return ApplyImpl2((std::index_sequence_for<Args...>*)nullptr, f, std::forward<Args>(args)...); }
#	define PROXY_APPLY_(...) ProxyApply(std::bool_constant<sizeof...(Args) == sizeof...(FuncArgs)>{}, __VA_ARGS__)
#else
#	define PROXY_APPLY_(...) ApplyImpl(__VA_ARGS__)
#endif
		// clang-format on

	public:
		template<typename F, typename... Args>
		FORCEINLINE static decltype(auto) Apply(const F& f, Args&&... args)
		{
			static_assert(sizeof...(Args) >= sizeof...(FuncArgs), "args count error");
			return PROXY_APPLY_(f, std::forward<Args>(args)...);
		}
#undef PROXY_APPLY_
	};
}  // namespace Details

// clang-format off
class Connection
{
public:
	Connection() = default;
	Connection(Connection&&) = default;
	Connection(const Connection&) = default;
	Connection& operator=(Connection&&) = default;
	Connection& operator=(const Connection&) = default;
	Connection(Details::TSlot<KeyType>&& Func, KeyType Seq) : DeleteFun(std::move(Func)), Key(Seq) {}
	bool IsValid() { return DeleteFun && Key; }
	KeyType GetKey() const { return Key; }
	void Disconnect()
	{
		check(IsInGameThread());
		if (IsValid()) DeleteFun(Key);
		DeleteFun = nullptr;
	}
	explicit operator KeyType() const { return KeyType(Key); }

protected:
	Details::TSlot<KeyType> DeleteFun;
	KeyType Key;
};

class AutoConnection final : public Connection
{
public:
	AutoConnection(Connection& Con) : Connection(Con) {}
	AutoConnection(Connection&& Con) : Connection(std::move(Con)) {}
	AutoConnection() = default;
	AutoConnection(AutoConnection&&) = default;
	AutoConnection& operator=(AutoConnection&&) = default;
	AutoConnection(const AutoConnection&) = delete;
	AutoConnection& operator=(const AutoConnection&) = delete;
	~AutoConnection() { Disconnect(); }
};

// auto disconnect signals
class FSignalsHandle
{
public:
	void DisconnectAll() { Handles.Reset(); }
	void Disconnect(KeyType Key) { Handles.RemoveAll([&](const AutoConnection& con) { return con.GetKey() == Key; }); }

private:
	template<typename...>
	friend class TSignal;
	TArray<AutoConnection> Handles;
};

struct UseDefaultId {};
struct UseDelegateId {};

template<typename... Args>
struct TSlotElem
{
	KeyType Seq;
	FWeakObjectPtr WeakObj;
	FWeakObjectPtr WatchedObj;

	using FSlot = Details::TUniqueSlot<Details::ForwardParam<Args>...>;
	FSlot Func;

	TSlotElem() = default;
	TSlotElem(KeyType Key) : Seq(Key) {}

	TSlotElem(TSlotElem&&) = default;
	TSlotElem& operator=(TSlotElem&&) = default;
	TSlotElem(const TSlotElem&) = delete;
	TSlotElem& operator=(const TSlotElem&) = delete;

	void SetLeftTimes(int32 InTimes) { Times = (InTimes < 0 ? -1 : InTimes); }
	int32 TestLeftTimes() { return Times > 0 ? Times-- : Times; }

private:
	int32 Times = -1;
};
// clang-format on

template<typename SlotElemType>
struct TSlotElemKeyFuncs : public BaseKeyFuncs<SlotElemType, KeyType, false>
{
	static FORCEINLINE KeyType GetSetKey(const SlotElemType& Element) { return Element.Seq; }
	static FORCEINLINE bool Matches(KeyType A, KeyType B) { return A == B; }
	static FORCEINLINE uint32 GetKeyHash(KeyType Key) { return GetTypeHash(Key); }
};

using FKeyArray = TArray<KeyType, TInlineAllocator<8>>;

template<typename FSlotElemType>
using TSlotElemments = TSet<FSlotElemType, TSlotElemKeyFuncs<FSlotElemType>>;
template<typename FSlotElemType>
TSlotElemments<FSlotElemType> GlobalSlots;

GENERICSTORAGES_API KeyType NextSequence();

template<typename... Args>
class TSignal
{
public:
	TSignal() = default;
	TSignal(TSignal&&) = default;
	TSignal& operator=(TSignal&&) = default;
	TSignal(const TSignal&) = delete;
	TSignal& operator=(const TSignal&) = delete;

	using FSlotElem = TSlotElem<Args...>;
	using FSlot = typename FSlotElem::FSlot;

#define SIGNALS_MULTI_THREAD_REMOVAL 1
	struct FSignalImpl
#if SIGNALS_MULTI_THREAD_REMOVAL
		final : public FUObjectArray::FUObjectDeleteListener
#endif
	{
		FSignalImpl() = default;
		FSignalImpl(FSignalImpl&&) = delete;
		FSignalImpl& operator=(FSignalImpl&&) = delete;
		FSignalImpl(const FSignalImpl&) = delete;
		FSignalImpl& operator=(const FSignalImpl&) = delete;

#if SIGNALS_MULTI_THREAD_REMOVAL
		~FSignalImpl()
		{
			if (bListened && UObjectInitialized())
				DisableListener();
		}
		virtual void NotifyUObjectDeleted(const UObjectBase* ObjectBase, int32 Index) override
		{
			auto Object = static_cast<const UObject*>(ObjectBase);
			OnWatchedObjectRemoved(Object);
		}

		void OnWatchedObjectRemoved(const UObject* Object)
		{
#	if ENGINE_MINOR_VERSION >= 23
			// TODO IsInGarbageCollectorThread()
			static TLockFreePointerListUnordered<FWeakObjectPtr, PLATFORM_CACHE_LINE_SIZE> GameThreadObjects;
			if (IsInGameThread())
#	else
			check(IsInGameThread());
#	endif
			{
#	if ENGINE_MINOR_VERSION >= 23
				if (!GameThreadObjects.IsEmpty())
				{
					TArray<FWeakObjectPtr*> Objs;
					GameThreadObjects.PopAll(Objs);
					for (auto a : Objs)
					{
						TSet<KeyType> Arr;
						if (WatchedObjects.RemoveAndCopyValue(*a, Arr))
						{
							for (auto& id : Arr)
								GetSlots().Remove(id);
							delete a;
						}
					}
				}
#	endif

				TSet<KeyType> Arr;
				if (WatchedObjects.RemoveAndCopyValue(Object, Arr))
				{
					for (auto& id : Arr)
						GetSlots().Remove(id);
				}
				if (WatchedObjects.Num() == 0)
					DisableListener();
			}
#	if ENGINE_MINOR_VERSION >= 23
			else
			{
				GameThreadObjects.Push(new FWeakObjectPtr(Object));
			}
#	endif
		}
		void OnUObjectArrayShutdown()
		{
			GetSlots().Empty();
			WatchedObjects.Empty();
			DisableListener();
		}
		void DisableListener()
		{
			if (bListened)
			{
				bListened = false;
				GUObjectArray.RemoveUObjectDeleteListener(this);
			}
		}
		void EnableListener()
		{
			if (!bListened)
			{
				bListened = true;
				GUObjectArray.AddUObjectDeleteListener(this);
			}
		}

		void AddWatchedObj(const UObject* WatchedObj, KeyType Key)
		{
			check(WatchedObj);
			EnableListener();
			WatchedObjects.FindOrAdd(WatchedObj).Add(Key);
		}
		void RemoveSlot(KeyType Key) { GetSlots().Remove(Key); }
		bool bListened = false;
#else
		void OnWatchedObjectRemoved(const UObject* Object)
		{
			TSet<KeyType> Arr;
			if (WatchedObjects.RemoveAndCopyValue(Object, Arr))
			{
				for (auto& id : Arr)
					RemoveSlot(id);
			}
		}
		void AddWatchedObj(const UObject* WatchedObj, KeyType Key)
		{
			check(WatchedObj);
			WatchedObjects.FindOrAdd(WatchedObj).Add(Key);
		}
		void RemoveSlot(KeyType Key) { GetSlots().Remove(Key); }
#endif

#if 1  // TODO
		TSlotElemments<FSlotElem> LocalSlots;
		TSlotElemments<FSlotElem>& GetSlots() { return LocalSlots; }
#else
		TSlotElemments<FSlotElem>& GetSlots() { return GlobalSlots<FSlotElem>; }
#endif

		TMap<FWeakObjectPtr, TSet<KeyType>> WatchedObjects;
		FKeyArray Listeners;
		TMap<FWeakObjectPtr, KeyType> WeakObjIds;

		bool AddWeakObj(const UObject* Obj, KeyType Key) { return WeakObjIds.Add(Obj, Key) == Key; }
		FSlotElem* FindSlot(KeyType Key) { return GetSlots().Find(Key); }

		bool IsAlive(KeyType Key)
		{
			auto Find = FindSlot(Key);
			return Find ? !Find->WeakObj.IsStale(true) : false;
		}

		void RemoveByKey(KeyType Key, bool bIncludeListeners = true)
		{
			if (auto Find = FindSlot(Key))
			{
				WeakObjIds.Remove(Find->WeakObj);

				RemoveSlot(Key);
				if (bIncludeListeners)
					Listeners.Remove(Key);
			}
		}

		void RemoveByObject(const UObject* Obj)
		{
			KeyType Key;
			if (WeakObjIds.RemoveAndCopyValue(FWeakObjectPtr(Obj), Key))
			{
				RemoveSlot(Key);
				Listeners.Remove(Key);
			}
		}

		FSlotElem* AddSlot(KeyType Key, FSlot&& slot, const UObject* WeakObj, const UObject* WatchedObj = nullptr)
		{
			if (WeakObj && !AddWeakObj(WeakObj, Key))
			{
				// oops! listen twice
				ensure(false);
				return nullptr;
			}

			auto& Ref = GetSlots()[GetSlots().Emplace(Key)];
			Ref.WeakObj = WeakObj;
			Ref.Func = std::move(slot);

			if (WatchedObj)
			{
				Ref.WatchedObj = WatchedObj;
				AddWatchedObj(WatchedObj, Key);
			}
			else
			{
				Listeners.Emplace(Key);
			}

			return &Ref;
		}
	};

	bool IsEmpty() const { return SignalImpl_->Listeners.Num() == 0; }

	template<typename R, typename T, typename... FuncArgs>
	inline std::enable_if_t<sizeof...(FuncArgs) == sizeof...(Args), FSlotElem*> Connect(T* const Obj, R (T::*const mem_fun)(FuncArgs...), const UObject* WatchedObj = nullptr)
	{
		static_assert(std::is_base_of<UObject, T>::value || std::is_base_of<FSignalsHandle, T>::value, "must use UObject or FSignalsHandle!");
		check(IsInGameThread() && Obj);
		return ConnectImpl(
			Details::TestSignalsHandle<T>{},
			Obj,
			[=](Details::ForwardParam<Args>... args) { return (Obj->*mem_fun)(static_cast<Args>(args)...); },
			WatchedObj);
	}

	template<typename R, typename T, typename... FuncArgs>
	inline std::enable_if_t<sizeof...(FuncArgs) != sizeof...(Args), FSlotElem*> Connect(T* const Obj, R (T::*const mem_fun)(FuncArgs...), const UObject* WatchedObj = nullptr)
	{
		static_assert(sizeof...(FuncArgs) < sizeof...(Args), "overflow");
		static_assert(std::is_base_of<UObject, T>::value || std::is_base_of<FSignalsHandle, T>::value, "must use UObject or FSignalsHandle!");
		check(IsInGameThread() && Obj);
		return ConnectImpl(
			Details::TestSignalsHandle<T>{},
			Obj,
			[=](Details::ForwardParam<Args>... args) { Details::Invoker<FuncArgs...>::Apply(mem_fun, Obj, Details::ForwardParam<Args>(args)...); },
			WatchedObj);
	}

	template<typename R, typename T, typename... FuncArgs>
	inline std::enable_if_t<sizeof...(FuncArgs) == sizeof...(Args), FSlotElem*> Connect(const T* const Obj, R (T::*const mem_fun)(FuncArgs...) const, const UObject* WatchedObj = nullptr)
	{
		static_assert(std::is_base_of<UObject, T>::value || std::is_base_of<FSignalsHandle, T>::value, "must use UObject or FSignalsHandle!");
		check(IsInGameThread() && Obj);
		return ConnectImpl(
			Details::TestSignalsHandle<T>{},
			Obj,
			[=](Details::ForwardParam<Args>... args) { return (Obj->*mem_fun)(static_cast<Args>(args)...); },
			WatchedObj);
	}

	template<typename R, typename T, typename... FuncArgs>
	inline std::enable_if_t<sizeof...(FuncArgs) != sizeof...(Args), FSlotElem*> Connect(const T* const Obj, R (T::*const mem_fun)(FuncArgs...) const, const UObject* WatchedObj = nullptr)
	{
		static_assert(sizeof...(FuncArgs) < sizeof...(Args), "overflow");
		static_assert(std::is_base_of<UObject, T>::value || std::is_base_of<FSignalsHandle, T>::value, "must use UObject or FSignalsHandle!");
		check(IsInGameThread() && Obj);
		return ConnectImpl(
			Details::TestSignalsHandle<T>{},
			Obj,
			[=](Details::ForwardParam<Args>... args) { Details::Invoker<FuncArgs...>::Apply(mem_fun, Obj, Details::ForwardParam<Args>(args)...); },
			WatchedObj);
	}

	template<typename T, typename F>
	FSlotElem* Connect(T* const Obj, F&& slot, const UObject* WatchedObj = nullptr)
	{
		static_assert(std::is_base_of<UObject, T>::value || std::is_base_of<FSignalsHandle, T>::value, "must use UObject or FSignalsHandle!");
		check(IsInGameThread() && Obj);
		using DF = std::decay_t<F>;
		return ConnectAnyImpl(Obj, std::forward<F>(slot), &DF::operator(), WatchedObj);
	}

	bool IsAlive(KeyType Key) const
	{
		check(IsInGameThread());
		return SignalImpl_->IsAlive(Key);
	}

	void Disconnect()
	{
		check(IsInGameThread());
		SignalImpl_ = MakeSignals();
	}
	void DisconnectAll() { Disconnect(); }
	void Disconnect(KeyType Key)
	{
		check(IsInGameThread());
		SignalImpl_->RemoveByKey(Key);
	}
	void Disconnect(const UObject* Obj)
	{
		check(IsInGameThread() && Obj);
		SignalImpl_->RemoveByObject(Obj);
	}

#ifndef GMP_SIGNAL_COMPATIBLE_WITH_DELEGATE_HANDLE
#	define GMP_SIGNAL_COMPATIBLE_WITH_DELEGATE_HANDLE 0
#endif

#if GMP_SIGNAL_COMPATIBLE_WITH_DELEGATE_HANDLE
	template<typename... Ts>
	void Disconnect(const TBaseDelegate<Ts...>& Handle)
	{
		check(IsInGameThread());
		Disconnect(GetDelegateHandleID(Handle));
	}
#endif

	Connection MakeConnection(KeyType Key)
	{
		if (!ensure(Key != 0))
			return Connection{};

		TWeakPtr<FSignalImpl> weak_self = SignalImpl_;
		auto DeleteFun = [weak_self](KeyType InKey) {
			auto SlotListImpl = weak_self.Pin();
			if (SlotListImpl)
				SlotListImpl->RemoveByKey(InKey);
		};
		return Connection{DeleteFun, Key};
	}

	void Fire(Args... args) const
	{
		check(IsInGameThread());
		ensureMsgf(SignalImpl_.IsUnique(), TEXT("maybe unsafe, should avoid reentry."));

		auto signal_impl_ref = SignalImpl_;
		FSignalImpl& signal_impl = *signal_impl_ref;
		auto CallbackIDs = signal_impl.Listeners;
		auto CallbackNums = CallbackIDs.Num();
		FKeyArray EraseIDs;
		for (auto Idx = 0; Idx < CallbackNums; ++Idx)
		{
			auto ID = CallbackIDs[Idx];
			auto Elem = signal_impl.FindSlot(ID);
			if (!Elem)
			{
				EraseIDs.Add(ID);
				continue;
			}

			auto& Listener = Elem->WeakObj;
			if (!Listener.IsStale(true))
			{
				switch (Elem->TestLeftTimes())
				{
					case 1:
						Elem->Func(Details::ForwardParam<Args>(args)...);
					case 0:
						EraseIDs.Add(ID);
						break;
					default:
						Elem->Func(Details::ForwardParam<Args>(args)...);
						break;
				};
			}
			else
			{
				EraseIDs.Add(ID);
			}
		}

		for (auto ID : EraseIDs)
			signal_impl.RemoveByKey(ID);
	}

	void FireWithSender(const UObject* Sender, Args... args) const
	{
		check(IsInGameThread());
		ensureMsgf(SignalImpl_.IsUnique(), TEXT("maybe unsafe, should avoid reentry."));

		auto signal_impl_ref = SignalImpl_;
		FSignalImpl& signal_impl = *signal_impl_ref;

		// FIXME: need keep the order?
		TArray<KeyType> CallbackIDs;
		auto Find = signal_impl.WatchedObjects.Find(Sender);
		if (Find)
			CallbackIDs.Append(Find->Array());

		CallbackIDs.Append(signal_impl.Listeners);

		FKeyArray EraseIDs;

		CallbackIDs.Sort();
		for (auto Idx = 0; Idx < CallbackIDs.Num(); ++Idx)
		{
			auto ID = CallbackIDs[Idx];
			auto Elem = signal_impl.FindSlot(ID);
			if (!Elem)
			{
				EraseIDs.Add(ID);
				continue;
			}

			auto& Listener = Elem->WeakObj;
			if (!Listener.IsStale(true))
			{
#if WITH_EDITOR
				// if mutli world in one process : PIE
				if (Listener.Get() && Sender && Listener.Get()->GetWorld() != Sender->GetWorld())
					continue;
#endif
				switch (Elem->TestLeftTimes())
				{
					case 1:
						Elem->Func(Details::ForwardParam<Args>(args)...);
					case 0:
						EraseIDs.Add(ID);
						break;
					default:
						Elem->Func(Details::ForwardParam<Args>(args)...);
						break;
				}
			}
			else
			{
				EraseIDs.Add(ID);
			}
		}

		if (EraseIDs.Num() > 0)
		{
			if (Find)
			{
				for (auto idx = EraseIDs.Num() - 1; idx >= 0; --idx)
				{
					auto ID = EraseIDs[idx];
					if (Find->Remove(ID) > 0)
					{
						signal_impl.RemoveSlot(ID);
						EraseIDs.RemoveAtSwap(idx);
					}
				}
				if (Find->Num() == 0)
					signal_impl.OnWatchedObjectRemoved(Sender);
			}

			for (auto ID : EraseIDs)
				signal_impl.RemoveByKey(ID);
		}
	}

private:
	template<typename T, typename R, typename F, typename FF, typename... FuncArgs>
	inline std::enable_if_t<sizeof...(FuncArgs) == sizeof...(Args), FSlotElem*> ConnectAnyImpl(const T* Obj, F&& slot, R (FF::*const)(FuncArgs...) const, const UObject* WatchedObj)
	{
		return ConnectImpl(Details::TestSignalsHandle<T>{}, Obj, std::forward<F>(slot), WatchedObj, GetSeq(slot));
	}

	template<typename T, typename R, typename F, typename FF, typename... FuncArgs>
	inline std::enable_if_t<sizeof...(FuncArgs) != sizeof...(Args), FSlotElem*> ConnectAnyImpl(const T* Obj, F&& slot, R (FF::*const)(FuncArgs...) const, const UObject* WatchedObj)
	{
		static_assert(sizeof...(FuncArgs) < sizeof...(Args), "overflow");
		return ConnectImpl(
			Details::TestSignalsHandle<T>{},
			Obj,
			[slot{std::forward<F>(slot)}](Details::ForwardParam<Args>... args) { Details::Invoker<FuncArgs...>::Apply(slot, Details::ForwardParam<Args>(args)...); },
			WatchedObj,
			GetSeq(slot));
	}

	template<typename T>
	auto ConnectImpl(std::true_type, T* const Obj, FSlot&& slot, const UObject* WatchedObj, KeyType Seq = 0)
	{
		static_assert(std::is_base_of<FSignalsHandle, T>::value, "must use FSignalHandle!");
		auto Key = ConnectImpl(NotSignalHandle(), (UObject*)nullptr, std::move(slot), WatchedObj, Seq);
		if (Key)
			Obj->Handles.Emplace(MakeConnection(Key));
		return Key;
	}

	template<typename T>
	auto ConnectImpl(std::false_type, T* const Obj, FSlot&& slot, const UObject* WatchedObj, KeyType Seq = 0)
	{
		auto Key = Seq ? Seq : GetSeq(slot);
		const bool bObjectType = std::is_base_of<UObject, T>::value;
		const UObject* WeakObj = bObjectType ? Obj : (const UObject*)nullptr;
		auto Item = SignalImpl_->AddSlot(Key, std::move(slot), WeakObj, WatchedObj);
		ensureAlwaysMsgf(Item, TEXT("connect twice on %s"), *GetNameSafe(WeakObj));
		return Item;
	}

private:
#if GMP_SIGNAL_COMPATIBLE_WITH_DELEGATE_HANDLE
	template<typename H>
	static KeyType GetDelegateHandleID(const H& handle)
	{
		static_assert(sizeof(handle) == sizeof(KeyType), "err");
		return *reinterpret_cast<const KeyType*>(&handle);
	}
	template<typename F>
	KeyType GetSeq(const F& f, std::enable_if_t<std::is_base_of<FDelegateBase, F>::value>* = nullptr)
	{
		return GetDelegateHandleID(f.GetHandle());
	}
	template<typename F>
	KeyType GetSeq(const F& f, std::enable_if_t<!std::is_base_of<FDelegateBase, F>::value>* = nullptr)
	{
		return GetNextSequence();
	}
	auto GetNextSequence()
	{
		// same to delegate handle id
		return GetDelegateHandleID(FDelegateHandle(FDelegateHandle::GenerateNewHandle));
	}
#else
	template<typename F>
	KeyType GetSeq(const F& f)
	{
		return NextSequence();
	}
#endif
	static auto MakeSignals()
	{
		auto SignalImpl = MakeShared<FSignalImpl>();
#if SIGNALS_MULTI_THREAD_REMOVAL && ENGINE_MINOR_VERSION < 23
		TWeakPtr<FSignalImpl> WeakImpl = SignalImpl;
		FCoreDelegates::OnPreExit.AddLambda([WeakImpl] {
			if (auto Impl = WeakImpl.Pin())
			{
				Impl->OnUObjectArrayShutdown();
			}
		});
#endif
		return SignalImpl;
	}

	TSharedPtr<FSignalImpl> SignalImpl_ = MakeSignals();
};

}  // namespace GS_SIGNALS

template<typename... Args>
using TSignal = GS_SIGNALS::TSignal<Args...>;

using FSignal = TSignal<>;
using FSignalsHandle = GS_SIGNALS::FSignalsHandle;
