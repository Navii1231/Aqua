#pragma once
#include "config.h"

class IndexTable : std::map<Index, std::map<Index, Index>>
{
public:
	IndexTable() = default;
	~IndexTable() = default;

	Index& Get(Index featureIdx, Index id) { return this->operator[](featureIdx)[id]; }
	const Index& Get(Index featureIdx, Index id) const { return at(featureIdx).at(id); }

	Index& operator()(Index featureIdx, Index id) { return this->operator[](featureIdx)[id]; }
	const Index& operator()(Index featureIdx, Index id) const { return at(featureIdx).at(id); }
};
