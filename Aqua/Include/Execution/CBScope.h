#pragma once
#include "GraphConfig.h"
#include "Graph.h"

AQUA_BEGIN
EXEC_BEGIN

class CBScope
{
public:
	CBScope(vk::CommandBuffer cmd, vk::CommandBufferUsageFlags cb_usage = vk::CommandBufferUsageFlagBits::eOneTimeSubmit)
		: mCmds(cmd)
	{
		mCmds.reset();
		mCmds.begin({ cb_usage });
	}

	~CBScope()
	{
		mCmds.end();
	}

	CBScope(const CBScope&) = delete;
	CBScope& operator=(const CBScope&) = delete;

private:
	vk::CommandBuffer mCmds;
};

EXEC_END
AQUA_END

