#pragma once

#include <BWAPI.h>

struct SBuildOrderEntry
{
	int _Supply;
	BWAPI::UnitType _UnitType;
};

struct SBuildOrder
{
	const char* pName;

	int _nEntries;
	SBuildOrderEntry* pEntries;
};

const SBuildOrder* GetBuildOrder(const char* pName);
int GetUnitCountForEntry(const SBuildOrder* pBuildOrder, int EntryIndex);
