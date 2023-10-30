#pragma once

#include "CoreTypes.h"

#include "Math/NumericLimits.h"
#include "Templates/UniquePtr.h"

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

inline bool ChunkingFile(const TCHAR* Filename, const TFunctionRef<void(const TArray64<uint8>&)>& Lambda, int32 InSize = 2048)
{
	TArray64<uint8> Buffer;
	Buffer.SetNumUninitialized(InSize);
	return ChunkingFile(Filename, Buffer, Lambda);
}

struct GENERICSTORAGES_API FMappedBuffer
{
public:
	FMappedBuffer(FGuid InId, uint32 InCapacity);

	bool IsValid() const;
	bool IsEmpty() const { return ReadIdx == WriteIdx; }

	bool WillWrap(uint32 InSize) const;
	bool WillFull(uint32 InSize) const;

	bool Write(const void* Value, uint32 InSize, bool bContinuous = false);
	uint32 ReadUntil(uint8 TestChar, TFunctionRef<void(uint8)> Op);
	uint32 ReadUntil(uint8 TestChar);

	FORCEINLINE uint32 Num() const { return IsWrap() ? (GetRegionSize() - WriteIdx + ReadIdx) : (WriteIdx - ReadIdx); }
	FORCEINLINE uint32 Capacity() const { return GetRegionSize(); }

	FORCEINLINE const uint8& Peek(uint32 Index) const
	{
		if (Index >= Capacity())
		{
			Index = Capacity() - 1;
		}
		return GetBuffer((ReadIdx + Index) % Capacity());
	}
	FORCEINLINE const uint8& operator[](uint32 Index) const { return Peek(Index); }

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
}  // namespace MIO
