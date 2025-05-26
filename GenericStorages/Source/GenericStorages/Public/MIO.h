#pragma once

#include "CoreTypes.h"

#include "Math/NumericLimits.h"
#include "Templates/UniquePtr.h"
#include "Templates/Function.h"
#include "Containers/ContainersFwd.h"

namespace MIO
{
template<typename ByteT>
class IMappedFileRegion
{
public:
	static_assert(sizeof(ByteT) == sizeof(char), "err");

	virtual ~IMappedFileRegion() {}
	virtual ByteT* GetMappedPtr() = 0;
	virtual int64 GetMappedSize() = 0;
	virtual FString& GetInfo() = 0;
};

GENERICSTORAGES_API TUniquePtr<IMappedFileRegion<uint8>> OpenMappedWrite(const TCHAR* Filename, int64 Offset = 0, int64 BytesToMap = MAX_int64, bool bPreloadHint = false);
GENERICSTORAGES_API TUniquePtr<IMappedFileRegion<const uint8>> OpenMappedRead(const TCHAR* Filename, int64 Offset = 0, int64 BytesToMap = 0, bool bPreloadHint = false);
GENERICSTORAGES_API bool ChunkingFile(const TCHAR* Filename, TArray64<uint8>& Buffer, const TFunctionRef<void(const TArray64<uint8>&)>& Lambda);
GENERICSTORAGES_API FString ConvertToAbsolutePath(FString InOutPath);
GENERICSTORAGES_API int32 ReadLines(const TCHAR* Filename, const TFunctionRef<void(const TArray<uint8>&)>& Lambda, char Dim = '\n');
GENERICSTORAGES_API int32 WriteLines(const TCHAR* Filename, const TArray<TArray<uint8>>& Lines, char Dim = '\n');

inline bool ChunkingFile(const TCHAR* Filename, const TFunctionRef<void(const TArray64<uint8>&)>& Lambda, int32 InSize = 2048)
{
	TArray64<uint8> Buffer;
	Buffer.SetNumUninitialized(InSize);
	return ChunkingFile(Filename, Buffer, Lambda);
}

class GENERICSTORAGES_API FMappedBuffer
{
public:
	FMappedBuffer(FGuid InId, uint32 InCapacity, const TCHAR* SubDir = nullptr);

	bool IsValid() const;
	bool IsEmpty() const { return ReadIdx == WriteIdx; }
	FORCEINLINE uint32 Num() const { return IsWrap() ? (GetRegionSize() - WriteIdx + ReadIdx) : (WriteIdx - ReadIdx); }
	FORCEINLINE uint32 Capacity() const { return GetRegionSize(); }
	uint32 GetBufferLength() const { return IsWrap() ? GetRegionSize() : GetRegionSize() - WriteIdx; }
	FString GetFilePath() { return Region ? Region->GetInfo() : TEXT(""); }

public:
	uint32 ReadUntil(uint8 TestChar, TFunctionRef<void(uint8)> Op);
	uint32 ReadUntil(uint8 TestChar);

	FORCEINLINE const uint8& Peek(uint32 Index) const
	{
		if (Index >= Capacity())
		{
			Index = Capacity() - 1;
		}
		return GetBuffer((ReadIdx + Index) % (Capacity() > 0 ? Capacity() : 1));
	}
	FORCEINLINE const uint8& operator[](uint32 Index) const { return Peek(Index); }

public:
	bool WillWrap(uint32 InSize) const;
	bool WillFull(uint32 InSize, bool bContinuous = false) const;
	uint8* Write(const void* Value, uint32 InSize, bool bContinuous = false);
	FORCEINLINE uint32 GetWirteIdx() const { return WriteIdx; }
	FORCEINLINE uint8* GetAddr(uint32 Idx = 0) const { return &GetBuffer(Idx); }

protected:
	uint32 WriteImpl(uint32 InLen, const void* InBuf);
	bool IsWrap() const { return WriteIdx < ReadIdx; }
	uint8& GetBuffer(int32 Idx) const;
	uint32 GetRegionSize() const;
	uint8* GetPtr() const;
	TUniquePtr<IMappedFileRegion<uint8>> Region;
	uint32 ReadIdx;
	uint32 WriteIdx;
};

GENERICSTORAGES_API void* OpenLockHandle(const TCHAR* Path, FString& ErrorCategory);
GENERICSTORAGES_API void CloseLockHandle(void* InHandle);

struct FProcessLockIndex
{
	int32 Index;
	int32 PIEIndex;
	FString AsSuffix(const FString& Prefix = TEXT("_")) const
	{
		if (!Index)
			return TEXT("");
		return Prefix + LexToString(Index);
	}
};
GENERICSTORAGES_API int32 GetProcessUniqueIndex();
GENERICSTORAGES_API TSharedPtr<FProcessLockIndex> GetGlobalSystemIndexHandle(const TCHAR* Key, int32 MaxTries = 1024);
GENERICSTORAGES_API FProcessLockIndex* GetGameInstanceIndexHandle(const UObject* InCtx, const TCHAR* Key = nullptr, int32 MaxTries = 1024);
}  // namespace MIO
