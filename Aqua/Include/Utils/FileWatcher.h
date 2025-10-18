#pragma once
#include "../Core/AqCore.h"

AQUA_BEGIN

enum FileEvent
{
	eNoAction                 = 0,
	eCreated                  = 1,
	eModified                 = 2,
	eDeleted                  = 3,
};

using FileCallback = std::function<void(FileEvent, const std::filesystem::directory_entry&)>;

// meant to work on a separate thread
// you can combine it with thread pool library
class FileWatcher
{
public:
	FileWatcher() = default;
	~FileWatcher() = default;

	// resets everything including the callback function
	inline void Reset(const std::string& path);

	void SetCallback(FileCallback&& fn) { mCallback = fn; }
	void SetMaxRecursion(int depth) { mMaxDepth = depth; }

	// not thread safe
	// 60 refreshes per second by default
	inline void Refresh(std::chrono::nanoseconds refreshRate = std::chrono::microseconds(16667)) const;

private:
	struct FileState
	{
		std::filesystem::directory_entry mEntry;
		bool mRedBlackMarker = false;

		bool operator==(const FileState& other) const { return mEntry.path() == other.mEntry.path(); }
		bool operator!=(const FileState& other) const { return !operator==(other); }
	};

	std::filesystem::path mDirectory; // absolute path

	mutable std::vector<FileState> mLiveEntries;

	int mMaxDepth = 1;
	mutable bool mRedBlackMarker = false;

	mutable std::chrono::time_point<std::chrono::high_resolution_clock> mNextRefreshPoint = std::chrono::high_resolution_clock::now();

	FileCallback mCallback;

private:
	inline FileEvent TestAction(const std::filesystem::directory_entry& entry) const;
	inline void WaitForNextRefresh(std::chrono::nanoseconds refreshRate) const;

	friend struct std::hash<FileState>;
};

AQUA_END

void AQUA_NAMESPACE::FileWatcher::Reset(const std::string& path)
{
	mDirectory = std::filesystem::absolute(path);
	mCallback = [](FileEvent, const std::filesystem::directory_entry&) {};
	Refresh(std::chrono::seconds(0));
}

void AQUA_NAMESPACE::FileWatcher::Refresh(std::chrono::nanoseconds rr /*= std::chrono::microseconds(16667)*/) const
{
	WaitForNextRefresh(rr);

	std::filesystem::recursive_directory_iterator iterator(mDirectory);

	// switch the marker to distinguish the deleted files
	mRedBlackMarker = !mRedBlackMarker;

	// testing for modification and creation
	for (const auto& entry : iterator)
	{
		if (iterator.depth() > mMaxDepth || entry.is_directory())
			continue;

		mCallback(TestAction(entry), entry);
	}

	// testing for deletion
	for (int64_t i = 0; i < static_cast<int64_t>(mLiveEntries.size()); i++)
	{
		const auto& entry = mLiveEntries[i];

		// the flag do not match, means the file watcher missed it likely deleted
		// todo: missed entry doesn't necessary mean the file was deleted
		if (entry.mRedBlackMarker != mRedBlackMarker)
		{
			mCallback(FileEvent::eDeleted, entry.mEntry);
			mLiveEntries.erase(mLiveEntries.begin() + i);
			i--;
		}
	}
}

AQUA_NAMESPACE::FileEvent AQUA_NAMESPACE::FileWatcher::TestAction(const std::filesystem::directory_entry& entry) const
{
	FileState fileState = { entry, mRedBlackMarker };

	auto storedIt = std::find_if(mLiveEntries.begin(), mLiveEntries.end(), 
		[&fileState](const FileState& state)
	{
		return state == fileState;
	});

	if (storedIt == mLiveEntries.end())
	{
		mLiveEntries.push_back(fileState);
		return FileEvent::eCreated;
	}

	if (storedIt->mEntry.last_write_time() != entry.last_write_time())
	{
		*storedIt = fileState;
		return FileEvent::eModified;
	}

	*storedIt = fileState;
	return FileEvent::eNoAction;
}

void AQUA_NAMESPACE::FileWatcher::WaitForNextRefresh(std::chrono::nanoseconds refreshRate) const
{
	if (refreshRate == std::chrono::seconds(0))
		return;

	std::this_thread::sleep_until(mNextRefreshPoint);
	mNextRefreshPoint = std::chrono::high_resolution_clock::now() + refreshRate;
}

