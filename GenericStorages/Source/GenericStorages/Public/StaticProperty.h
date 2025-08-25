// Copyright GenericStorages, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "UnrealCompatibility.h"

namespace GenericStorages
{
#define PROPERTY_FIND_OR_ADD WITH_EDITOR
	
	template<typename T>
	const FProperty* StaticProperty();
	namespace Details
	{
		GENERICSTORAGES_API UObject* GetPropertiesHolder();
	#if UE_5_05_OR_LATER
		template<typename T, typename... TArgs>
		T* NewNativeProperty(const FName& ObjName, uint64 Flag, TArgs... Args)
		{
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			return new T(GetPropertiesHolder(), ObjName, (RF_Public | RF_DuplicateTransient | RF_Transient | RF_MarkAsNative | RF_MarkAsRootSet));
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}
	#else
		FORCEINLINE auto GetPropElementSize(const FProperty* Prop) {return Prop->ElementSize; }

		template<typename T, typename... TArgs>
		T* NewNativeProperty(const FName& ObjName, uint64 Flag, TArgs... Args)
		{
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			return new T(GetPropertiesHolder(), ObjName, (RF_Public | RF_DuplicateTransient | RF_Transient | RF_MarkAsNative | RF_MarkAsRootSet), 0, (EPropertyFlags)Flag, Args...);
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}
	#endif
		
		inline auto EncodeTypeName(FName TypeName)
		{
			return TypeName;
		}
		inline auto DecodeTypeName(FName TypeName)
		{
			return TypeName;
		}

		template<typename C>
		bool VerifyPropertyType(const FProperty* Prop, C* Class)
		{
			return (Prop && ensureAlways(Prop->IsA(Class)));
		}
		template<typename T>
		bool VerifyPropertyType(const FProperty* Prop)
		{
			return VerifyPropertyType(Prop, T::StaticClass());
		}
		GENERICSTORAGES_API const FProperty*& FindOrAddProperty(FName PropTypeName);
		GENERICSTORAGES_API const FProperty* FindOrAddProperty(FName PropTypeName, const FProperty* Prop);
		template<typename T>
		const T*& FindOrAddProperty(FName PropTypeName)
		{
			static_assert(std::is_base_of_v<FProperty, T>, "T must derive from FProperty");
			const FProperty*& Prop = FindOrAddProperty(PropTypeName);
	#if !UE_BUILD_SHIPPING
			if (!VerifyPropertyType<T>(Prop))
				Prop = nullptr;
	#endif
			return *reinterpret_cast<T**>(&Prop);
		}
#if PROPERTY_FIND_OR_ADD
		template<typename T, typename F>
		const T*& FindOrAddProperty(FName PropTypeName, const F& Ctor)
		{
			static_assert(std::is_base_of_v<FProperty, T>, "T must derive from FProperty");
			const FProperty*& Prop = FindOrAddProperty(PropTypeName);
			if (!Prop)
				Prop = Ctor();
#if !UE_BUILD_SHIPPING
			if (!VerifyPropertyType<T>(Prop))
				Prop = nullptr;
#endif
			return *reinterpret_cast<T**>(&Prop);
		}
#else
		template<typename T, typename F>
		FORCEINLINE const T* FindOrAddProperty(FName PropTypeName, const F& Ctor)
		{
			static const T* Prop = Ctor();
			return Prop;
		}
#endif
		
		const FProperty* GetProperty(const FName& ObjName);

		template<typename T>
		struct TType2PropertyMap;
	#define DECLARE_TYPE_2_PROPERTY_MAPPING(T, PropType) template<> struct TType2PropertyMap<T> { using Type = PropType; static FName GetFName() { return FName(ITS::TypeStr<T>()); } };
		DECLARE_TYPE_2_PROPERTY_MAPPING(bool, FBoolProperty)
		DECLARE_TYPE_2_PROPERTY_MAPPING(uint8, FByteProperty)
		DECLARE_TYPE_2_PROPERTY_MAPPING(int32, FIntProperty)
		DECLARE_TYPE_2_PROPERTY_MAPPING(int64, FInt64Property)
		DECLARE_TYPE_2_PROPERTY_MAPPING(uint32, FUInt32Property)
		DECLARE_TYPE_2_PROPERTY_MAPPING(uint64, FUInt64Property)
		DECLARE_TYPE_2_PROPERTY_MAPPING(float, FFloatProperty)
		DECLARE_TYPE_2_PROPERTY_MAPPING(double, FDoubleProperty)
		DECLARE_TYPE_2_PROPERTY_MAPPING(FString, FStrProperty)
		DECLARE_TYPE_2_PROPERTY_MAPPING(FName, FNameProperty)
		DECLARE_TYPE_2_PROPERTY_MAPPING(FText, FTextProperty)
		DECLARE_TYPE_2_PROPERTY_MAPPING(FWeakObjectPtr, FWeakObjectProperty)
		DECLARE_TYPE_2_PROPERTY_MAPPING(FLazyObjectPtr, FLazyObjectProperty)
		DECLARE_TYPE_2_PROPERTY_MAPPING(FSoftObjectPath, FSoftObjectProperty)
		DECLARE_TYPE_2_PROPERTY_MAPPING(FSoftClassPath, FSoftClassProperty)
		
		template<typename T>
		struct TBasicPropertyTraits
		{
			using PropertyType = typename TType2PropertyMap<T>::Type;
			static const auto* GetProperty()
			{
				static auto ConstructProp = []
				{
					return NewNativeProperty<PropertyType>(TType2PropertyMap<T>::GetFName(), CPF_HasGetValueTypeHash);
				};	
				return FindOrAddProperty<PropertyType>(TType2PropertyMap<T>::GetFName(), ConstructProp); 
			}
		};

		template<typename T>
		struct TSructPropertyTraits
		{
			static const FStructProperty* GetProperty()
			{
				static auto Struct = TBaseStructure<T>::Get();
				static auto ConstructProp = []
				{
					return NewNativeProperty<FStructProperty>(Struct->GetFName(), CPF_HasGetValueTypeHash, const_cast<UScriptStruct*>(Struct));
				};
				return FindOrAddProperty<FStructProperty>(Struct->GetFName(), ConstructProp);
			}
		};
		template<typename T>
		struct TObjectPropertyTraits : std::false_type
		{
			
		};
		struct FObjectPropertyTraits : std::true_type
		{
			template<typename T, typename P>
			static const P* GetProperty()
			{
				static_assert(std::is_base_of_v<FObjectPropertyBase, P>, "P must derive from FObjectPropertyBase");
				static auto Cls = T::StaticClass();
				static FName ClsName = Cls->GetFName();
				static auto ConstructProp = []
				{
					auto Prop = NewNativeProperty<P>(ClsName, CPF_HasGetValueTypeHash);
					Prop->PropertyClass = Cls;
					if constexpr (std::is_base_of_v<FClassProperty, P> || std::is_base_of_v<FSoftClassProperty, P>)
					{
						Prop->SetMetaClass(Cls);
					}
					return Prop;
				};
				return FindOrAddProperty<P>(ClsName, ConstructProp);
			}
		};
		template<typename T>
		struct TObjectPropertyTraits<TObjectPtr<T>> : public FObjectPropertyTraits
		{
#if UE_5_04_OR_LATER
			static const FObjectProperty* GetProperty()
			{
				return FObjectPropertyTraits::GetProperty<T, FObjectProperty>(T::StaticClass()->GetFName());
			}
#else
			static const FObjectPtrProperty* GetProperty()
			{
				return FObjectPropertyTraits::GetProperty<T, FObjectPtrProperty>(T::StaticClass()->GetFName());
			}
#endif
		};

		template<typename T>
		struct TObjectPropertyTraits<TWeakObjectPtr<T>> : public FObjectPropertyTraits
		{
			static const FWeakObjectProperty* GetProperty()
			{
				return FObjectPropertyTraits::GetProperty<T, FWeakObjectProperty>(T::StaticClass()->GetFName());
			}
		};
		template<> struct TObjectPropertyTraits<FWeakObjectPtr> : public TObjectPropertyTraits<TWeakObjectPtr<UObject>>{};

		template<typename T>
		struct TObjectPropertyTraits<TLazyObjectPtr<T>> : public FObjectPropertyTraits
		{
			static const FLazyObjectProperty* GetProperty()
			{
				return FObjectPropertyTraits::GetProperty<T, FLazyObjectProperty>(T::StaticClass()->GetFName());
			}
		};
		template<> struct TObjectPropertyTraits<FLazyObjectPtr> : public TObjectPropertyTraits<TLazyObjectPtr<UObject>>{};

		template<typename T>
		struct TObjectPropertyTraits<TSoftObjectPtr<T>> : public FObjectPropertyTraits
		{
			static const FSoftObjectProperty* GetProperty()
			{
				return FObjectPropertyTraits::GetProperty<T, FSoftObjectProperty>(T::StaticClass()->GetFName());
			}
		};
		template<> struct TObjectPropertyTraits<FSoftObjectPath> : public TObjectPropertyTraits<TSoftObjectPtr<UObject>>{};

		template<typename T>
		struct TObjectPropertyTraits<TSubclassOf<T>> : public FObjectPropertyTraits
		{
			static const FSoftObjectProperty* GetProperty()
			{
				return FObjectPropertyTraits::GetProperty<T, FClassProperty>(T::StaticClass()->GetFName());
			}
		};
		template<typename T>
		struct TObjectPropertyTraits<TSoftClassPtr<T>> : public FObjectPropertyTraits
		{
			static const FSoftObjectProperty* GetProperty()
			{
				return FObjectPropertyTraits::GetProperty<T, FSoftClassProperty>(T::StaticClass()->GetFName());
			}
		};
		template<> struct TObjectPropertyTraits<FSoftClassPath> : public TObjectPropertyTraits<TSoftClassPtr<UObject>>{};

		template<typename T>
		struct TEnumPropertyTraits
		{
			static const FEnumProperty* GetProperty()
			{
				static auto Enum = StaticEnum<T>();
				static auto ConstructProp = []
				{
					return NewNativeProperty<FEnumProperty>(Enum->GetFName(), CPF_HasGetValueTypeHash, const_cast<UEnum*>(Enum));
				};
				return FindOrAddProperty<FEnumProperty>(Enum->GetFName(), ConstructProp);
			}
		};

		template<typename T>
		struct TArrayPropertyTraits;
		template<typename T>
		struct TArrayPropertyTraits<TArray<T>>
		{
			static const FArrayProperty* GetProperty()
			{
				static auto ElmProp = StaticProperty<T>();
				static FName ArrayPropName = *FString::Printf(TEXT("TArray<%s>"), *ElmProp->GetFName().ToString());
				static auto ConstructProp = []
				{
					auto Prop = NewNativeProperty<FArrayProperty>(ArrayPropName, CPF_HasGetValueTypeHash, EArrayPropertyFlags::None);
					Prop->AddCppProperty(ElmProp);
					return Prop;
				};
				return FindOrAddProperty<FArrayProperty>(ArrayPropName, ConstructProp);
			}
		};


		template<typename T>
		struct TSetPropertyTraits;
		template<typename T>
		struct TSetPropertyTraits<TSet<T>>
		{
			static const FSetProperty* GetProperty()
			{
				static auto ElmProp = StaticProperty<T>();
				static FName SetPropName = *FString::Printf(TEXT("TSet<%s>"), *ElmProp->GetFName().ToString());
				static auto ConstructProp = []
				{
					auto Prop = NewNativeProperty<FSetProperty>(SetPropName, CPF_HasGetValueTypeHash);
					Prop->AddCppProperty(ElmProp);
					return Prop;
				};
				return FindOrAddProperty<FSetProperty>(SetPropName, ConstructProp);
			}
		};

		template<typename T>
		struct TMapPropertyTraits;
		template<typename K, typename V>
		struct TMapPropertyTraits<TMap<K, V>>
		{
			static const FMapProperty* GetProperty()
			{
				static auto KeyProp = StaticProperty<K>();
				static auto ValueProp = StaticProperty<V>();
				static FName MapPropName = *FString::Printf(TEXT("TMap<%s,%s>"), *KeyProp->GetFName().ToString(), *ValueProp->GetFName().ToString());
				static auto ConstructProp = []
				{
					auto Prop = NewNativeProperty<FMapProperty>(MapPropName, CPF_HasGetValueTypeHash, EMapPropertyFlags::None);
					Prop->AddCppProperty(const_cast<FProperty*>(KeyProp));
					Prop->AddCppProperty(const_cast<FProperty*>(ValueProp));
					return Prop;
				};
				return FindOrAddProperty<FMapProperty>(MapPropName);
			}
		};
	}

	template<typename TT>
	const FProperty* StaticProperty()
	{
		using namespace Details;
		using T = std::remove_cvref_t<TT>;
		if constexpr (TModels_V<CStaticStructProvider, T>)
		{
			return TSructPropertyTraits<T>::GetProperty();
		}
		else if constexpr (TIsSame<std::remove_pointer_t<T>, UClass>::Value)
		{
			return FObjectPropertyTraits::GetProperty<T, FClassProperty>(UObject::StaticClass()->GetFName());
		}
		else if constexpr (TIsDerivedFrom<std::remove_pointer_t<T>, UObject>::Value)
		{
			return FObjectPropertyTraits::GetProperty<T, FObjectProperty>();
		}
		else if constexpr (TObjectPropertyTraits<T>::value)
		{
			return TObjectPropertyTraits<T>::GetProperty();
		}
		else if constexpr (TIsEnum<T>::Value)
		{
			return TEnumPropertyTraits<T>::GetProperty();
		}
		else if constexpr (TIsTArray<T>::Value)
		{
			return TArrayPropertyTraits<T>::GetProperty();
		}
		else if constexpr (TIsTSet<T>::Value)
		{
			return TSetPropertyTraits<T>::GetProperty();
		}
		else if constexpr (TIsTMap<T>::Value)
		{
			return TMapPropertyTraits<T>::GetProperty();
		}
		else
		{
			return TBasicPropertyTraits<T>::GetProperty();
		}
	}
}

struct FInstancedPropertyVal
{
	struct FDeleter
	{
		void operator()(FInstancedPropertyVal* Ptr) const
		{
			if (Ptr && Ptr->Prop)
			{
				Ptr->Prop->DestroyValue(Ptr->Addr);
			}
			FMemory::Free(Ptr);
		}
	};
	using FInstancedPropertyValPtr = TUniquePtr<FInstancedPropertyVal, FDeleter>;

	static FInstancedPropertyValPtr MakeUnique(const FProperty* Prop)
	{
		return FInstancedPropertyValPtr(MakeRaw(Prop));
	}

	template<typename T, typename = std::enable_if_t<!std::is_base_of_v<FProperty, T>>>
	static FInstancedPropertyValPtr MakeDuplicate(const T& Val)
	{
		auto Prop = GenericStorages::StaticProperty<T>();
		return MakeDuplicate(Prop, std::addressof(Val));
	}
	
	static FInstancedPropertyValPtr MakeDuplicate(const FProperty* Prop, const void* Src)
	{
		return FInstancedPropertyValPtr(MakeRaw(Prop, Src));
	}
#if UE_5_05_OR_LATER
	static FORCEINLINE auto GetPropElementSize(const FProperty* Prop) { return Prop->GetElementSize(); }
#else
	static FORCEINLINE auto GetPropElementSize(const FProperty* Prop) { return Prop->ElementSize; }
#endif
	const void* GetMemory() const
	{
		return Addr;
	}
protected:
	static FORCEINLINE FInstancedPropertyVal* MakeRaw(const FProperty* Prop, const void* Src = nullptr)
	{
		auto Ptr = static_cast<FInstancedPropertyVal*>(FMemory::Malloc(GetPropElementSize(Prop) + sizeof(Prop),
																		 Prop->GetMinAlignment()));
		new (Ptr) FInstancedPropertyVal(Prop, Src);
		return Ptr;
	}
	FInstancedPropertyVal(const FProperty* InProp, const void* Src = nullptr): Prop(InProp)
	{
		Prop->InitializeValue(Addr);
		if (Src)
		{
			Prop->CopyCompleteValue(Addr, Src);
		}
	}
	~FInstancedPropertyVal() = default;
	const FProperty* Prop;
	size_t Addr[0];
};
using FInstancedPropertyValPtr = FInstancedPropertyVal::FInstancedPropertyValPtr;