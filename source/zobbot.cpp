#include "zobbot.h"

#include <iostream>

using namespace BWAPI;
using namespace Filter;

void CAgent::DoAction()
{
	if (_pAgentStrategy)
	{
		_pAgentStrategy->DoAction(this);
	}
}

bool CAgent::IsActive()
{
	if (_Unit->isLockedDown() || _Unit->isMaelstrommed() || _Unit->isStasised())
		return false;

	if (_Unit->isLoaded() || !_Unit->isPowered() || _Unit->isStuck())
		return false;

	if (!_Unit->isCompleted() || _Unit->isConstructing())
		return false;

	return true;
}

void CAgent::ResetStrategy()
{
	delete _pAgentStrategy;
	_pAgentStrategy = nullptr;
}

CProbe::CGatherStrategy::CGatherStrategy(Base Base)
	: _Base( Base )
{
}

void CProbe::CGatherStrategy::Do(CProbe* pProbe)
{
	BWAPI::Unit Unit = pProbe->_Unit;

	if (Unit->isIdle() && _Base->HasNexus())
	{
		if (Unit->isCarryingGas() || Unit->isCarryingMinerals())
		{
			Unit->returnCargo();
		}
		else
		{
			if (_Base->HasMineralField())
			{
				BWAPI::Unit MineralField = _Base->GetMineralField();
				if (!Unit->gather(MineralField))
				{
					Broodwar << Broodwar->getLastError() << std::endl;
				}
			}
		}
	}
}

CProbe::CBuildStrategy::CBuildStrategy(BuildAction BuildAction)
	: _BuildAction(BuildAction)
{
}

void CProbe::CBuildStrategy::Do(CProbe* pProbe)
{
	if (_BuildAction->IsCompleted())
	{
		pProbe->ResetStrategy();
		return;
	}

	BWAPI::Unit Unit = pProbe->_Unit;
	BWAPI::UnitType Type = _BuildAction->GetUnitType();

	if (CTactician::GetInstance()->CanAfford(Type))
	{
		TilePosition BuildLocation = Broodwar->getBuildLocation(Type, Unit->getTilePosition());
		if (BuildLocation)
		{
			Broodwar->registerEvent([BuildLocation, Type](Game*)
			{
				Broodwar->drawBoxMap(Position(BuildLocation), Position(BuildLocation + Type.tileSize()), Colors::Blue);
			}, nullptr, Type.buildTime() + 100);
		
			_bStartedBuildAction = true;
			Unit->build(Type, BuildLocation);
		}
	}
}

void CNexus::CBuildProbe::Do(CNexus* pNexus)
{
	BWAPI::Unit Unit = pNexus->_Unit;
	BWAPI::UnitType WorkerType = Unit->getType().getRace().getWorker();
	if (Unit->isIdle() && CTactician::GetInstance()->CanAffordUnallocated(WorkerType) && !Unit->train(WorkerType))
	{
		Position pos = Unit->getPosition();
		Error lastErr = Broodwar->getLastError();
		Broodwar->registerEvent([pos, lastErr](Game*) { Broodwar->drawTextMap(pos, "%c%s", Text::White, lastErr.c_str()); }, nullptr, Broodwar->getLatencyFrames());
	}
}

//void CNexus::DoAction()
//{
//	if (_Unit->isIdle() && !_Unit->train(_Unit->getType().getRace().getWorker()))
//	{
//		Position pos = _Unit->getPosition();
//		Error lastErr = Broodwar->getLastError();
//		Broodwar->registerEvent([pos, lastErr](Game*) { Broodwar->drawTextMap(pos, "%c%s", Text::White, lastErr.c_str()); }, nullptr, Broodwar->getLatencyFrames());
//	
//		UnitType supplyProviderType = _Unit->getType().getRace().getSupplyProvider();
//		static int lastChecked = 0;
//	
//		if (lastErr == Errors::Insufficient_Supply &&
//			lastChecked + 400 < Broodwar->getFrameCount() &&
//			Broodwar->self()->incompleteUnitCount(supplyProviderType) == 0)
//		{
//			lastChecked = Broodwar->getFrameCount();
//	
//			Unit supplyBuilder = _Unit->getClosestUnit(GetType == supplyProviderType.whatBuilds().first &&
//				(IsIdle || IsGatheringMinerals) &&
//				IsOwned);
//	
//			if (supplyBuilder)
//			{
//				if (supplyProviderType.isBuilding())
//				{
//					TilePosition targetBuildLocation = Broodwar->getBuildLocation(supplyProviderType, supplyBuilder->getTilePosition());
//					if (targetBuildLocation)
//					{
//						Broodwar->registerEvent([targetBuildLocation, supplyProviderType](Game*)
//						{
//							Broodwar->drawBoxMap(Position(targetBuildLocation), Position(targetBuildLocation + supplyProviderType.tileSize()), Colors::Blue);
//						}, nullptr, supplyProviderType.buildTime() + 100);
//	
//						supplyBuilder->build(supplyProviderType, targetBuildLocation);
//					}
//				}
//				else
//				{
//					supplyBuilder->train(supplyProviderType);
//				}
//			}
//		}
//	}
//}

CTactician* CTactician::_pInstance = nullptr;

void CTactician::Update()
{
	_AgentManager.RefreshAgents();

	UpdateBases();
	SetProbeStrategies();
	UpdateBuildActions();

	_AgentManager.DoActions();

	//Broodwar->drawTextScreen(200, 40, "Total supply: %d", _Me->supplyTotal() / 2 );
	//Broodwar->drawTextScreen(200, 60, "Used supply: %d", _Me->supplyUsed() / 2 );
}

void CTactician::AllocateResourcesForUnitType(BWAPI::UnitType UnitType)
{
	AllocateResources( UnitType.mineralPrice(), UnitType.gasPrice() );
}

void CTactician::DeallocateResourcesForUnitType(BWAPI::UnitType UnitType)
{
	DeallocateResources(UnitType.mineralPrice(), UnitType.gasPrice());
}

int CTactician::GetUnallocatedMinerals()
{
	int Minerals, Gas;
	GetUnallocatedResources(Minerals, Gas);
	return Minerals;
}

int CTactician::GetUnallocatedGas()
{
	int Minerals, Gas;
	GetUnallocatedResources(Minerals, Gas);
	return Gas;
}

void CTactician::GetResources(int& MineralsOut, int& GasOut)
{
	MineralsOut = _Me->minerals();
	GasOut = _Me->gas();
}

void CTactician::GetUnallocatedResources(int& MineralsOut, int& GasOut)
{
	GetResources(MineralsOut, GasOut);
	MineralsOut -= _AllocatedMinerals;
	GasOut -= _AllocatedGas;
}

bool CTactician::CanAffordUnallocated(BWAPI::UnitType UnitType)
{
	int Minerals, Gas;
	GetUnallocatedResources(Minerals, Gas);
	return Minerals >= UnitType.mineralPrice() && Gas >= UnitType.gasPrice();
}

void CTactician::UnitCreated(BWAPI::Unit Unit)
{
	for (std::vector<BuildAction>::iterator It = _BuildActions.begin(); It != _BuildActions.end(); ++It) 
	{
		if ((*It)->GetUnitType() == Unit->getType())
		{
			(*It)->SetCompleted(true);
			_BuildActions.erase(It);
			break;
		}
	}
}

int CTactician::GetUnitTypeCount(BWAPI::UnitType UnitType, bool bCountUnfinished)
{
	int UnitCount = 0;
	if (bCountUnfinished)
	{
		UnitCount += CountBuildActionFor(UnitType);
	}

	for (const BWAPI::Unit& Unit : _Me->getUnits())
	{
		if (Unit->getType() == UnitType)
		{
			UnitCount++;
		}
	}

	return UnitCount;
}

bool bBuildOrderDone = false;
void CTactician::UpdateBuildActions()
{
	if (_pBuildOrder)
	{
		int SupplyUsed = _Me->supplyUsed() / 2;
		for (int i = 0; i < _pBuildOrder->_nEntries && SupplyUsed >= _pBuildOrder->pEntries[i]._Supply; i++)
		{
			SBuildOrderEntry& Entry = _pBuildOrder->pEntries[i];
			int UnitCount = GetUnitCountForEntry(_pBuildOrder, i);
			int OwnedUnits = GetUnitTypeCount(Entry._UnitType, true);

			if (OwnedUnits < UnitCount)
			{
				bBuildOrderDone = false;
				AddBuildAction(Entry._UnitType);
			}

			if (i == _pBuildOrder->_nEntries - 1)
			{
				if (!bBuildOrderDone)
				{
					Broodwar->sendText("Build order finished!");
					bBuildOrderDone = true;
				}
			}
		}
	}

	if (bBuildOrderDone)
	{
		if (NeedPylon() && !HasBuildActionFor(UnitTypes::Protoss_Pylon))
		{
			AddBuildAction(UnitTypes::Protoss_Pylon);
		}
	}

	// Assign probes to build actions
	for (BuildAction BuildAction : _BuildActions)
	{
		BWAPI::UnitType UnitType = BuildAction->GetUnitType();
		if (!UnitType.isBuilding())
			continue;

		bool bHasAction = false;
		for (auto It : _AgentManager._Probes)
		{
			Agent<CProbe> Probe = It.second;
			CProbe::CBuildStrategy* pBuildStrategy = Probe->AccessStrategy<CProbe::CBuildStrategy>();
			if (pBuildStrategy && pBuildStrategy->AccessBuildAction() == BuildAction)
			{
				bHasAction = true;
				break;
			}
		}

		if (!bHasAction)
		{
			Agent<CProbe> Probe = GetBuilder();
			if (Probe)
			{
				Probe->SetStrategy<CProbe::CBuildStrategy>(BuildAction);
			}
		}
	}
}

void CTactician::AddBuildAction(BWAPI::UnitType UnitType)
{
	_BuildActions.push_back( std::make_shared<CBuildAction>( UnitType ) );
}

namespace
{
	int GetSupplyThreshold(int SupplyTotal)
	{
		if (SupplyTotal <= 9)
			return 1;
		else if (SupplyTotal <= 17)
			return 2;
		else
			return 3;
	}
}

bool CTactician::NeedPylon()
{
	if (_Me->supplyTotal() == 200)
	{
		return false;
	}

	int IncompletePylons = _Me->incompleteUnitCount( _Me->getRace().getSupplyProvider() );
	int SupplyTotal = (_Me->supplyTotal() / 2) + IncompletePylons * 8;
	int SupplyUsed = _Me->supplyUsed() / 2;
	return (SupplyTotal - SupplyUsed) <= GetSupplyThreshold(SupplyTotal);
}

bool CTactician::HasBuildActionFor(BWAPI::UnitType UnitType)
{
	for (BuildAction BuildAction : _BuildActions)
	{
		if (BuildAction->GetUnitType() == UnitType)
		{
			return true;
		}
	}

	return false;
}

int CTactician::CountBuildActionFor(BWAPI::UnitType UnitType)
{
	int Count = 0;
	for (BuildAction BuildAction : _BuildActions)
	{
		if (BuildAction->GetUnitType() == UnitType)
		{
			Count++;
		}
	}

	return Count;
}

bool CTactician::CanAfford(BWAPI::UnitType UnitType)
{
	int Minerals, Gas;
	GetResources(Minerals, Gas);
	return Minerals >= UnitType.mineralPrice() && Gas >= UnitType.gasPrice();
}

void CTactician::OnStart()
{
	SetBuildOrder("9/9 Gateways");
	_Me = Broodwar->self();
}

Agent<CProbe> CTactician::GetBuilder()
{
	for (auto It : _AgentManager._Probes)
	{
		Agent<CProbe> Probe = It.second;
		//if (Probe->HasStrategyType<CProbe::CGatherStrategy>() && !Probe->_Unit->isGatheringGas() &&
		//	!Probe->_Unit->isGatheringMinerals() && !Probe->_Unit->isCarryingMinerals() && !Probe->_Unit->isCarryingGas())
		if (Probe->HasStrategyType<CProbe::CGatherStrategy>() && !Probe->_Unit->isCarryingMinerals() && !Probe->_Unit->isCarryingGas())
		{
			return Probe;
		}
	}

	return nullptr;
}

void CTactician::SetBuildOrder(const char* pName)
{
	const SBuildOrder* pBuildOrder = GetBuildOrder( pName );
	if (pBuildOrder)
	{
		Broodwar->sendText("Setting build order '%s'", pBuildOrder->pName);
		_pBuildOrder = pBuildOrder;
	}
}

void CTactician::UpdateBases()
{
	for (auto It : _AgentManager._Nexuses)
	{
		Agent<CNexus> Nexus = It.second;
		if (Nexus && Nexus->IsActive())
		{
			bool bExistingBase = false;

			BWAPI::Position Position = Nexus->_Unit->getPosition();
			for (Base Base : _Bases)
			{
				if (Base->GetPosition() == Position)
				{
					Nexus->SetBase(Base);
					bExistingBase = true;
					break;
				}
			}

			if (!bExistingBase)
			{
				Base Base = std::make_shared<CBase>(Position);
				_Bases.push_back(Base);
				Nexus->SetBase(Base);
			}
		}
	}

	for (Base Base : _Bases)
	{
		Base->Update();
	}
}

void CTactician::SetProbeStrategies()
{
	// Assign gather to all probes that doesn't have a strategy
	for (auto It : _AgentManager._Probes)
	{
		Agent<CProbe> Probe = It.second;
		if (Probe && Probe->IsActive() && !Probe->HasStrategy() )
		{
			Agent<CNexus> Nexus = CAgentManager::AccessClosest( _AgentManager._Nexuses, Probe->_Unit->getPosition() );
			if (Nexus)
			{
				Probe->SetStrategy<CProbe::CGatherStrategy>(Nexus->GetBase());
			}
			else
				break;
		}
	}
}

void CAgentManager::RefreshAgents()
{
	for (const BWAPI::Unit& Unit : Broodwar->self()->getUnits())
	{
		RefreshAgent( Unit );
	}
}

void CAgentManager::DoActions()
{
	for (auto It : _AllAgents)
	{
		if (!It.second->IsActive())
			continue;

		It.second->DoAction();
	}
}

void CAgentManager::RemoveAgent(const BWAPI::Unit Unit)
{
	int Id = Unit->getID();

	//Broodwar->sendText("Removing agent with Id = %d", Id);

	RemoveAgent(_AllAgents, Id);

	if (Unit->getType().isWorker())
	{
		RemoveAgent(_Probes, Id);
	}
	else if (Unit->getType().isResourceDepot())
	{
		RemoveAgent(_Nexuses, Id);
	}
}

void CAgentManager::RefreshAgent(const BWAPI::Unit Unit)
{
	if (Unit->getType().isWorker())
	{
		SetAgent( _Probes, Unit );
	}
	else if (Unit->getType().isResourceDepot())
	{
		SetAgent( _Nexuses, Unit );
	}
	else
	{
		//Broodwar->sendText("Zobbot: Could not identify agent");
	}
}

void CZobbot::onStart()
{
	Broodwar->sendText("*static voice*: ZOBOOT ACTIVATEEED");
	Broodwar << "The map is " << Broodwar->mapName() << "!" << std::endl;
	Broodwar->enableFlag(Flag::UserInput);
	Broodwar->setCommandOptimizationLevel(2);

	if (Broodwar->isReplay())
	{
		Broodwar << "The following players are in this replay:" << std::endl;

		Playerset players = Broodwar->getPlayers();
		for (auto p : players)
		{
			if (!p->isObserver())
				Broodwar << p->getName() << ", playing as " << p->getRace() << std::endl;
		}
	}
	else
	{
		if (Broodwar->enemy())
			Broodwar << "The matchup is " << Broodwar->self()->getRace() << " vs " << Broodwar->enemy()->getRace() << std::endl;
	}

	CTactician::CreateInstance()->OnStart();
}

void CZobbot::onEnd(bool isWinner)
{
	if (isWinner)
	{
	}
}

void CZobbot::onFrame()
{
	Broodwar->drawTextScreen(200, 0, "FPS: %d", Broodwar->getFPS());
	//Broodwar->drawTextScreen(200, 20, "Average FPS: %f", Broodwar->getAverageFPS());
	Broodwar->drawTextScreen(200, 20, "APM: %d", Broodwar->getAPM());

	if (Broodwar->isReplay() || Broodwar->isPaused() || !Broodwar->self())
		return;

	if (Broodwar->getFrameCount() % Broodwar->getLatencyFrames() != 0)
		return;

	CTactician::GetInstance()->Update();
}

void CZobbot::onSendText(std::string text)
{
	Broodwar->sendText("%s", text.c_str());
}

void CZobbot::onReceiveText(BWAPI::Player player, std::string text)
{
	Broodwar << player->getName() << " said \"" << text << "\"" << std::endl;
}

void CZobbot::onPlayerLeft(BWAPI::Player player)
{
	Broodwar->sendText("Goodbye %s!", player->getName().c_str());
}

void CZobbot::onNukeDetect(BWAPI::Position target)
{
	if (target)
	{
		Broodwar << "Nuclear Launch Detected at " << target << std::endl;
	}
	else
	{
		Broodwar->sendText("Where's the nuke?");
	}
}

void CZobbot::onUnitDiscover(BWAPI::Unit unit)
{
}

void CZobbot::onUnitEvade(BWAPI::Unit unit)
{
}

void CZobbot::onUnitShow(BWAPI::Unit unit)
{
}

void CZobbot::onUnitHide(BWAPI::Unit unit)
{
}

void CZobbot::onUnitCreate(BWAPI::Unit Unit)
{
	if (Broodwar->isReplay())
	{
		if (Unit->getType().isBuilding() && !Unit->getPlayer()->isNeutral())
		{
			int seconds = Broodwar->getFrameCount() / 24;
			int minutes = seconds / 60;
			seconds %= 60;
			Broodwar->sendText("%.2d:%.2d: %s creates a %s", minutes, seconds, Unit->getPlayer()->getName().c_str(), Unit->getType().c_str());
		}
	}
	else
	{
		CTactician::GetInstance()->UnitCreated(Unit);
	}
}

void CZobbot::onUnitDestroy(BWAPI::Unit Unit)
{
	CTactician::GetInstance()->GetAgentManager().RemoveAgent( Unit );
}

void CZobbot::onUnitMorph(BWAPI::Unit Unit)
{
	if (Broodwar->isReplay())
	{
		if (Unit->getType().isBuilding() && !Unit->getPlayer()->isNeutral())
		{
			int seconds = Broodwar->getFrameCount() / 24;
			int minutes = seconds / 60;
			seconds %= 60;
			Broodwar->sendText("%.2d:%.2d: %s morphs a %s", minutes, seconds, Unit->getPlayer()->getName().c_str(), Unit->getType().c_str());
		}
	}
}

void CZobbot::onUnitRenegade(BWAPI::Unit unit)
{
}

void CZobbot::onSaveGame(std::string gameName)
{
	Broodwar << "The game was saved to \"" << gameName << "\"" << std::endl;
}

void CZobbot::onUnitComplete(BWAPI::Unit unit)
{
}

bool CBase::HasNexus()
{
	BWAPI::Unitset Units = Broodwar->getUnitsInRadius( _NexusPosition, 10, IsResourceDepot && IsOwned );
	return Units.size() > 0;
}

void CBase::Update()
{
	_MineralFields.clear();

	BWAPI::Unitset MineralFields = Broodwar->getUnitsInRadius(_NexusPosition, 300, IsMineralField);
	for (const BWAPI::Unit& Unit : MineralFields)
	{
		_MineralFields.push_back(Unit);
	}
}

BWAPI::Unit CBase::GetMineralField()
{
	_CurrentMineralField = _CurrentMineralField % _MineralFields.size();
	return _MineralFields[_CurrentMineralField++];
}

bool CBase::HasMineralField()
{
	return _MineralFields.size() > 0;
}

CBuildAction::CBuildAction(BWAPI::UnitType UnitType)
	: _UnitType(UnitType)
{
	Broodwar->sendText("Started build action '%s'", _UnitType.getName().c_str());
	CTactician::GetInstance()->AllocateResourcesForUnitType(_UnitType);
}

CBuildAction::~CBuildAction()
{
	Broodwar->sendText("Finished build action '%s'", _UnitType.getName().c_str());
	CTactician::GetInstance()->DeallocateResourcesForUnitType(_UnitType);
}

