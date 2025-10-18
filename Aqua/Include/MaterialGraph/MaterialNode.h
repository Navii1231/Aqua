#pragma once
#include "../Core/AqCore.h"

AQUA_BEGIN

struct MaterialNode;

enum class Platform
{
	eVulkan            = 0,
};

enum class SocketState
{
	eOpen              = 0,
	eConnected         = 1,
};

using SocketValues = std::vector<std::string>;

// No child nodes, holds a single value
struct ValueNode
{
	ValueNode() = default;

	ValueNode(const std::string& type, const std::string& value)
	{ mType = type; mValue = value; }

	void SetValue(const std::string& value)
	{ mValue = value; }

	std::string mType;
	std::string mValue;
};

class MaterialSocket
{
public:
	void Connect(std::shared_ptr<MaterialNode>& connection)
	{
		State = SocketState::eConnected;
		Connection = connection;
	}

	void AddValue(const std::string& type, const std::string& value)
	{
		Connection = std::shared_ptr<MaterialNode>();

		State = SocketState::eOpen;
		Value = ValueNode(type, value);
	}

	void SetValue(const std::string& value)
	{ 
		Connection = std::shared_ptr<MaterialNode>();

		State = SocketState::eOpen;
		Value.mValue = value;
	}

private:
	std::shared_ptr<MaterialNode> Connection;
	ValueNode Value;
	SocketState State;
	uint32_t SktIdx;
};

struct MaterialNode
{
	MaterialNode(Platform ptForm = Platform::eVulkan)
		: mPtForm(ptForm) {}

	virtual ~MaterialNode() = default;

	virtual std::string GenerateCode(const SocketValues& input) const = 0;

	MaterialSocket& operator[](const std::string& name) { return mChildNodes[name]; }
	const MaterialSocket& operator[](const std::string& name) const { return mChildNodes.at(name); }

	std::string mName;
	std::string mOutputInfo;
	SocketValues mValueStrs;
	std::unordered_map<std::string, MaterialSocket> mChildNodes;
	Platform mPtForm = Platform::eVulkan;
};

AQUA_END
