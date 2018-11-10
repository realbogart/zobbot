#include <string>
#include <unordered_map>

#include "buildorders.h"

using namespace std;

unordered_map<string, SBuildOrder> BuildOrders;
void RegisterBuildOrder(const SBuildOrder& BuildOrder)
{
	BuildOrders.emplace(BuildOrder.pName, BuildOrder);
}

struct SRegisterHelper
{
	SRegisterHelper(const SBuildOrder& BuildOrder)
	{
		RegisterBuildOrder(BuildOrder);
	}
};

#define CONCAT_(x,y) x##y
#define CONCAT(x,y) CONCAT_(x,y)
#define RegisterBuildOrder(Entries, Name) static SRegisterHelper CONCAT(bo_, __COUNTER__)( { Name, sizeof(Entries) / sizeof(Entries[0]), Entries } );

SBuildOrderEntry Description_12Nexus[] = {
	{8, BWAPI::UnitTypes::Protoss_Pylon},
	{12, BWAPI::UnitTypes::Protoss_Nexus},
	{14, BWAPI::UnitTypes::Protoss_Gateway},
	{15, BWAPI::UnitTypes::Protoss_Assimilator},
	{15, BWAPI::UnitTypes::Protoss_Zealot},
	{17, BWAPI::UnitTypes::Protoss_Cybernetics_Core},
	{17, BWAPI::UnitTypes::Protoss_Gateway}
};

SBuildOrderEntry Description_99Gateways[] = {
	{8, BWAPI::UnitTypes::Protoss_Pylon},
	{9, BWAPI::UnitTypes::Protoss_Gateway},
	{9, BWAPI::UnitTypes::Protoss_Gateway},
	{11, BWAPI::UnitTypes::Protoss_Zealot},
	{13, BWAPI::UnitTypes::Protoss_Pylon},
	{13, BWAPI::UnitTypes::Protoss_Zealot},
	{15, BWAPI::UnitTypes::Protoss_Zealot}
};

RegisterBuildOrder(Description_12Nexus, "12Nexus");
RegisterBuildOrder(Description_99Gateways, "9/9 Gateways");

const SBuildOrder* GetBuildOrder(const char* pName)
{
	auto It = BuildOrders.find(pName);
	if (It != BuildOrders.end())
	{
		return &It->second;
	}

	return nullptr;
}

int GetUnitCountForEntry(const SBuildOrder* pBuildOrder, int EntryIndex)
{
	int UnitCount = 0;

	// Starting nexus
	if (pBuildOrder->pEntries[EntryIndex]._UnitType == BWAPI::UnitTypes::Protoss_Nexus)
	{
		UnitCount++;
	}

	BWAPI::UnitType UnitType = pBuildOrder->pEntries[EntryIndex]._UnitType;
	for (int i = 0; i <= EntryIndex; i++)
	{
		if (pBuildOrder->pEntries[i]._UnitType == UnitType)
		{
			UnitCount++;
		}
	}

	return UnitCount;
}
