#pragma once
#include "Config.h"

VK_BEGIN

// A basic logger class for error reporting
class Logger
{
public:
	enum class Level
	{
		eTrace          = 1,
		eInfo           = 2,
		eWarn           = 3,
		eError          = 4,
		eCritical       = 5,
	};

public:
	Logger(Level loggingLevel = Level::eTrace)
		: mLevel(loggingLevel) {}

	void PushPrefixMsg()
	{
		switch (mLevel)
		{
			case Level::eTrace:
				mStream << "[Trace] --> ";
				break;
			case Level::eInfo:
				mStream << "[Info] --> ";
				break;
			case Level::eWarn:
				mStream << "[Warn] --> ";
				break;
			case Level::eError:
				mStream << "[Error] --> ";
				break;
			case Level::eCritical:
				mStream << "[Critical] --> ";
				break;
			default:
				mStream << "[Unknown] --> ";
				break;

		}
	}

	void PrintMsg() const
	{
		std::cout << mStream.str();
	}

	template <typename LogType>
	void PushLog(const LogType& loggingInfo)
	{
		mStream << loggingInfo;
	}

	explicit operator std::string() const { return mStream.str(); }

private:
	std::stringstream mStream;

	Level mLevel = Level::eTrace;
};

template<typename LogType>
inline Logger& operator<<(Logger& logger, const LogType& loggingInfo)
{
	logger.PushLog(loggingInfo);
	return logger;
}

template <typename BooleanConvertible>
void CheckAssert(BooleanConvertible cond, const std::string& fileName, int lineNo, const std::string& msg)
{
	if (!bool(cond))
	{
		std::cout << "In File: " << fileName << "\n" << "Line Number: " << lineNo << "\n" << msg << std::endl;
#if _DEBUG
		_CrtDbgReport(_CRT_ASSERT, fileName.c_str(), lineNo, nullptr, msg.c_str());
#else
		std::cin.get();
#endif
	}
}

#if _DEBUG

#define _VK_ASSERT(cond, msgStream)   \
	VK_NAMESPACE::Logger _CONCAT(logger, __LINE__)(VK_NAMESPACE::Logger::Level::eCritical); \
	if(!bool(cond))                                                                         \
	{                                                                                       \
		_CONCAT(logger, __LINE__) << "In function: " __FUNCTION__ << "\n\n[Critical]: " << msgStream; \
		VK_NAMESPACE::CheckAssert(cond, __FILE__, __LINE__, std::string(_CONCAT(logger, __LINE__)));  \
	}                                                                                       \
	if(!bool(cond)) _CrtDbgBreak()

#else

#define _VK_ASSERT(cond, msgStream)

#endif

VK_END
