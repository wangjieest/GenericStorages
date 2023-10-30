#include "MIO.h"

#include "GenericStoragesLog.h"
#include "mio/mmap.hpp"

namespace MIO
{
template<typename ByteT, typename MapType = std::conditional_t<std::is_const<ByteT>::value, mio::ummap_source, mio::ummap_sink>>
class FMappedFileRegionImpl final
	: public IMappedFileRegion<ByteT>
	, private MapType
{
	static_assert(sizeof(ByteT) == sizeof(char), "err");
	virtual ByteT* GetMappedPtr() override { return MapType::data(); }
	virtual int64 GetMappedSize() override { return MapType::mapped_length(); }
	virtual FString& GetInfo() override { return FileInfo; }

	FString FileInfo;

public:
	~FMappedFileRegionImpl() = default;
	FMappedFileRegionImpl(const TCHAR* Filename, int64 Offset = 0, int64 BytesToMap = MAX_int64, bool bPreloadHint = false)
	{
		std::error_code ErrCode;
		FileInfo = Filename;
		MapType::map(Filename, Offset, BytesToMap, ErrCode);

		if (ErrCode)
		{
			UE_LOG(LogGenericStorages, Error, TEXT("OpenMapped Error : [%s] %d(%s)"), Filename, ErrCode.value(), ANSI_TO_TCHAR(ErrCode.message().c_str()));
		}
	}
};

TUniquePtr<IMappedFileRegion<const uint8>> OpenMappedRead(const TCHAR* Filename, int64 Offset /*= 0*/, int64 BytesToMap /*= 0*/, bool bPreloadHint /*= false*/)
{
	return MakeUnique<FMappedFileRegionImpl<const uint8>>(Filename, Offset, BytesToMap, bPreloadHint);
}

TUniquePtr<IMappedFileRegion<uint8>> OpenMappedWrite(const TCHAR* Filename, int64 Offset /*= 0*/, int64 BytesToMap /*= MAX_int64*/, bool bPreloadHint /*= false*/)
{
	return MakeUnique<FMappedFileRegionImpl<uint8>>(Filename, Offset, BytesToMap, bPreloadHint);
}

bool ChunkingFile(const TCHAR* Filename, TArray64<uint8>& Buffer, const TFunctionRef<void(const TArray64<uint8>&)>& Lambda)
{
	check(Filename);
	if (!Buffer.Num())
		Buffer.SetNumUninitialized(2048);

	TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(Filename));
	if (ensure(Reader))
	{
		int64 LeftSize = Reader->TotalSize();
		while (LeftSize > 0)
		{
			auto ActualSize = LeftSize > Buffer.Num() ? Buffer.Num() : LeftSize;
			Buffer.SetNumUninitialized(ActualSize, false);
			Reader->Serialize(Buffer.GetData(), ActualSize);
			Lambda(Buffer);
			LeftSize -= ActualSize;
		}
		return true;
	}
	return false;
}

GENERICSTORAGES_API void* OpenLockHandle(const TCHAR* Path, FString& ErrorCategory)
{
	ensure(FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(Path)));
	return mio::OpenLockHandle(Path, ErrorCategory);
}
GENERICSTORAGES_API void CloseLockHandle(void* InHandle)
{
	if (InHandle)
		mio::CloseLockHandle(InHandle);
}

FMappedBuffer::FMappedBuffer(FGuid InId, uint32 InCapacity)
	: ReadIdx(0)
	, WriteIdx(0)
{
	auto RegionSize = FMath::RoundUpToPowerOfTwo(FMath::Max(PLATFORM_IOS ? 4096u * 4 : 4096u, InCapacity));
	Region = MIO::OpenMappedWrite(*FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Mapped"), InId.IsValid() ? InId.ToString() : FGuid::NewGuid().ToString()), 0, RegionSize);
}

bool FMappedBuffer::IsValid() const
{
	return Region.IsValid();
}

bool FMappedBuffer::WillWrap(uint32 InSize) const
{
	return ensure(InSize < GetRegionSize()) && (WriteIdx + InSize >= GetRegionSize());
}

bool FMappedBuffer::WillFull(uint32 InSize) const
{
	return ensure(InSize < GetRegionSize()) && (Num() + InSize < GetRegionSize());
}

bool FMappedBuffer::Write(const void* InVal, uint32 InSize, bool bContinuous)
{
	bool bTestFull = WillFull(InSize);
	if (!WillWrap(InSize))
	{
		FMemory::Memcpy(&GetBuffer(WriteIdx), InVal, InSize);
		WriteIdx += InSize;
	}
	else if (bContinuous)
	{
		FMemory::Memcpy(&GetBuffer(WriteIdx), InVal, GetRegionSize() - WriteIdx);
		WriteIdx += InSize - (GetRegionSize() - WriteIdx);
	}
	else
	{
		auto FirstPart = GetRegionSize() - WriteIdx;
		auto LeftSize = InSize - FirstPart;
		FMemory::Memcpy(&GetBuffer(WriteIdx), InVal, FirstPart);
		FMemory::Memcpy(&GetBuffer(0), (uint8*)InVal + FirstPart, LeftSize);
		WriteIdx = LeftSize;
	}

	if (bTestFull)
		ReadIdx = (WriteIdx + 1) % Capacity();
	return true;
}

uint32 FMappedBuffer::ReadUntil(uint8 TestChar, TFunctionRef<void(uint8)> Op)
{
	if (!IsWrap())
	{
		for (auto i = ReadIdx; i < WriteIdx; ++i)
		{
			auto Char = GetBuffer(i);
			if (Char == TestChar)
				return i - ReadIdx;
			Op(Char);
		}
	}
	else
	{
		for (auto i = ReadIdx; i < GetRegionSize(); ++i)
		{
			auto Char = GetBuffer(i);
			if (GetBuffer(i) == TestChar)
				return i - ReadIdx;
			Op(Char);
		}
		for (auto i = 0u; i < WriteIdx; ++i)
		{
			auto Char = GetBuffer(i);
			if (GetBuffer(i) == TestChar)
				return i + (GetRegionSize() - ReadIdx);
			Op(Char);
		}
	}
	return 0u;
}

uint32 FMappedBuffer::ReadUntil(uint8 TestChar)
{
	return ReadUntil(TestChar, [](auto) {});
}

uint32 FMappedBuffer::WriteImpl(uint32 InLen, const void* InBuf)
{
	const bool bIsEmplty = IsEmpty();
	if (bIsEmplty)
	{
		*reinterpret_cast<decltype(InLen)*>(GetPtr() + WriteIdx) = InLen;
		WriteIdx += sizeof(InLen);
		FMemory::Memcpy(&GetBuffer(WriteIdx), InBuf, InLen);
		WriteIdx += InLen;
	}
	return 0u;
}

uint8& FMappedBuffer::GetBuffer(int32 Idx) const
{
	return *(GetPtr() + Idx);
}

uint32 FMappedBuffer::GetRegionSize() const
{
	return Region ? Region->GetMappedSize() : 0;
}

uint8* FMappedBuffer::GetPtr() const
{
	check(IsValid());
	return Region->GetMappedPtr();
}

}  // namespace MIO
