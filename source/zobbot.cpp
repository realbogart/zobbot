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

int CScoutStrategy::NumScouts = 0;

CScoutStrategy::CScoutStrategy()
{
	NumScouts++;
}

CScoutStrategy::~CScoutStrategy()
{
	NumScouts--;
}

void CScoutStrategy::Do(CAgent* pAgent)
{
	if (!_bHasTarget || !pAgent->_Unit->isMoving())
		UpdateTarget(pAgent);

	const CVision& Vision = CTactician::GetInstance()->GetVision();
	if (pAgent->_Unit->isIdle() || Broodwar->isVisible(_TargetPosition))
	{
		UpdateTarget(pAgent);
	}
}

void CScoutStrategy::UpdateTarget(CAgent* pAgent)
{
	BWAPI::TilePosition OldTarget = _TargetPosition;
	const CVision& Vision = CTactician::GetInstance()->GetVision();
	if (Vision.GetEnemyMain(_TargetPosition) || Vision.GetUnexploredStartLocation(_TargetPosition))
	{
		if (_TargetPosition != OldTarget)
		{
			BWAPI::Position PixelPosition(_TargetPosition);
			pAgent->_Unit->move(PixelPosition);
			_bHasTarget = true;
		}
	}
}

void CAttackClosest::Do(CAgent* pAgent)
{
	BWAPI::Unit Unit = pAgent->_Unit;
	const CVision& Vision = CTactician::GetInstance()->GetVision();

	if (Unit->isIdle())
	{
		BWAPI::Position Position;
		if (CTactician::GetInstance()->GetAttackTarget(Position))
		{
			Unit->attack(Position);
		}
		else
		{
			// Scout target
		}
	}
}

void CProbe::CGatherStrategy::Do(CProbe* pProbe)
{
	BWAPI::Unit Unit = pProbe->_Unit;

	if (!_bGatherAction || (Unit->isIdle() && _Base->HasNexus()) )
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
				else
				{
					_bGatherAction = true;
				}
			}
			else if (true)
			{
				// Transfer
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

	TilePosition BuildLocation = _BuildAction->HasLocation() ? _BuildAction->GetBuildLocation() : Broodwar->getBuildLocation(Type, Unit->getTilePosition());
	if (BuildLocation)
	{
		if (!Broodwar->isVisible(BuildLocation))
		{
			if (Unit->isIdle() || Unit->isGatheringMinerals() || Unit->isGatheringGas())
			{
				Unit->move(BWAPI::Position(BuildLocation));
			}
		}
		else if (_LastChecked + 20 < Broodwar->getFrameCount() && CTactician::GetInstance()->CanAfford(Type))
		{
			_LastChecked = Broodwar->getFrameCount();

			Broodwar->printf(" Building %s at: %d, %d", _BuildAction->GetUnitType().getName().c_str(), BuildLocation.x, BuildLocation.y);
			Broodwar->registerEvent([BuildLocation, Type](Game*)
			{
				Broodwar->drawBoxMap(Position(BuildLocation), Position(BuildLocation + Type.tileSize()), Colors::Blue);
			}, nullptr, Type.buildTime() + 100);

			_bStartedBuildAction = true;
			Unit->build(Type, BuildLocation);
		}
	}
}

void CZealot::CAttackMain::Do(CZealot* pZealot)
{
	if (CTactician::GetInstance()->GetAgentManager()._Zealots.size() < 14)
		return;

	BWAPI::Unit Unit = pZealot->_Unit;
	BWAPI::Unitset Nearby = Unit->getUnitsInRadius(100, IsEnemy);
	const CVision& Vision = CTactician::GetInstance()->GetVision();
	BWAPI::TilePosition EnemyMain;
	
	if (Vision.GetEnemyMain(EnemyMain))
	{
		if (Unit->isIdle())
		{
			Position EnemyMainPixel(EnemyMain);
			if (Unit->getDistance(EnemyMainPixel) < 20)
			{
				BWAPI::Unit EnemyUnit = Unit->getClosestUnit(IsEnemy && !IsFlying && IsVisible);
				if(Unit->canAttack(EnemyUnit))
					Unit->attack(EnemyUnit->getPosition());
			}
			else
			{
				Unit->attack(EnemyMainPixel);
			}
		}
	}
}

void CNexus::CBuildProbe::Do(CNexus* pNexus)
{
	if (CTactician::GetInstance()->GetAgentManager()._Probes.size() > 60)
		return;

	BWAPI::Unit Unit = pNexus->_Unit;
	BWAPI::UnitType WorkerType = Unit->getType().getRace().getWorker();
	if (Unit->isIdle() && CTactician::GetInstance()->CanAffordUnallocated(WorkerType) && !Unit->train(WorkerType))
	{
		Position pos = Unit->getPosition();
		Error lastErr = Broodwar->getLastError();
		Broodwar->registerEvent([pos, lastErr](Game*) { Broodwar->drawTextMap(pos, "%c%s", Text::White, lastErr.c_str()); }, nullptr, Broodwar->getLatencyFrames());
	}
}

void CGateway::CBuildUnits::Do(CGateway* pGateway)
{
	BWAPI::Unit Unit = pGateway->_Unit;
	if (Unit->isIdle())
	{
		BuildAction BuildAction = CTactician::GetInstance()->AccessBuildAction(UnitTypes::Protoss_Zealot);
		if (BuildAction)
		{
			Unit->train(BuildAction->GetUnitType());
		}
		else if (CTactician::GetInstance()->CanAffordUnallocated(UnitTypes::Protoss_Zealot) && !Unit->train(UnitTypes::Protoss_Zealot))
		{
			Position pos = Unit->getPosition();
			Error lastErr = Broodwar->getLastError();
			Broodwar->registerEvent([pos, lastErr](Game*) { Broodwar->drawTextMap(pos, "%c%s", Text::White, lastErr.c_str()); }, nullptr, Broodwar->getLatencyFrames());
		}
	}
}

CTactician* CTactician::_pInstance = nullptr;

void CTactician::Update()
{
	_AgentManager.RefreshAgents();

	_Vision.Update();

	UpdateBases();
	SetProbeStrategies();
	UpdateBuildActions();
	UpdateScouts();

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

bool CTactician::GetAttackTarget(const BWAPI::Position& Position)
{
	return false;
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

BuildAction CTactician::AccessBuildAction(BWAPI::UnitType UnitType)
{
	for (BuildAction BuildAction : _BuildActions)
	{
		if (BuildAction->GetUnitType() == UnitType)
		{
			return BuildAction;
		}
	}

	return nullptr;
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

bool CTactician::GetNextExpansion(BWTA::BaseLocation* pFromLocation, BWAPI::TilePosition& PositionOut)
{
	Broodwar->printf("Searching for next expansion");

	struct SBase
	{
		BWAPI::TilePosition _Position;

		double _Distance;

		bool _bStartLocation;
		bool _bMineralOnly;
	};

	std::vector<SBase> Bases;

	for (BWTA::BaseLocation* pBaseLocation : BWTA::getBaseLocations())
	{
		SBase Base = {
			pBaseLocation->getTilePosition(),
			pBaseLocation->getGroundDistance(pFromLocation),
			pBaseLocation->isStartLocation(),
			pBaseLocation->isMineralOnly()
		};

		if(!pBaseLocation->isIsland())
			if(!Broodwar->isVisible(Base._Position) || Broodwar->getUnitsOnTile(Base._Position, IsResourceDepot).size() == 0)
				Bases.push_back(Base);
	}

	if (Bases.size() > 0)
	{
		sort(Bases.begin(), Bases.end(), [](const SBase& A, const SBase& B) -> bool
		{
			return ((!A._bMineralOnly && B._bMineralOnly) ||
				((A._bMineralOnly == B._bMineralOnly) && (A._Distance < B._Distance)));
		});

		PositionOut = Bases.front()._Position;
		return true;
	}

	return false;
}

BWTA::BaseLocation* CTactician::GetClosestBaseLocation(const BWAPI::Position& Position)
{
	Broodwar->printf("Searching for base location");

	double MinDistance = DBL_MAX;
	BWTA::BaseLocation* pClosestBaseLocation = nullptr;

	for (BWTA::BaseLocation* pBaseLocation : BWTA::getBaseLocations())
	{
		double Distance = pBaseLocation->getPosition().getDistance(Position);
		if (Distance < MinDistance)
		{
			MinDistance = Distance;
			pClosestBaseLocation = pBaseLocation;
		}
	}

	return pClosestBaseLocation;
}

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
				_bBuildOrderDone = false;
				AddBuildAction(Entry._UnitType);
			}

			if (i == _pBuildOrder->_nEntries - 1)
			{
				if (!_bBuildOrderDone)
				{
					Broodwar->printf("Build order finished!");
					_bBuildOrderDone = true;
				}
			}
		}
	}

	if (_bBuildOrderDone)
	{
		if (NeedPylon() && !HasBuildActionFor(UnitTypes::Protoss_Pylon))
		{
			AddBuildAction(UnitTypes::Protoss_Pylon);
		}
		else if (NeedExpansion())
		{
			BWAPI::TilePosition TilePosition;
			if (GetNextExpansion( _Bases.front()->GetBaseLocation(), TilePosition))
			{
				Broodwar->printf("Found expansion location!");
				AddBuildAction(BWAPI::UnitTypes::Protoss_Nexus, TilePosition);
			}
			Broodwar->printf("Time to expand!");
		}
		else if (NeedGateway())
		{
			BuildSingleOfType(UnitTypes::Protoss_Gateway);
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

void CTactician::AddBuildAction(BWAPI::UnitType UnitType, const BWAPI::TilePosition& TilePosition )
{
	_BuildActions.push_back( std::make_shared<CBuildAction>( UnitType, TilePosition) );
}

void CTactician::AddBuildAction(BWAPI::UnitType UnitType)
{
	_BuildActions.push_back( std::make_shared<CBuildAction>( UnitType ) );
}

void CTactician::BuildSingleOfType(BWAPI::UnitType UnitType)
{
	if(!HasBuildActionFor(UnitType))
		AddBuildAction(UnitTypes::Protoss_Gateway);
}

namespace
{
	int GetSupplyThreshold(int SupplyTotal)
	{
		if (SupplyTotal <= 9)
			return 1;
		else if (SupplyTotal <= 17)
			return 2;
		else if (SupplyTotal <= 33)
			return 5;
		else
			return 8;
	}
}

bool CTactician::NeedGateway()
{
	return GetUnallocatedMinerals() > 1000;
}

bool CTactician::NeedExpansion()
{
	if (HasBuildActionFor(BWAPI::UnitTypes::Protoss_Nexus))
		return false;

	if ( _Me->supplyUsed() / 2 > 40 && _AgentManager._Nexuses.size() < 2 ||
		_Me->supplyUsed() / 2 > 80 && _AgentManager._Nexuses.size() < 3 ||
		_Me->supplyUsed() / 2 > 120)
	{
		return true;
	}

	return false;
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
	//SetBuildOrder("12Nexus");
	_Me = Broodwar->self();
	_Vision.Init();
}

Agent<CProbe> CTactician::GetBuilder()
{
	for (auto It : _AgentManager._Probes)
	{
		Agent<CProbe> Probe = It.second;
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
		Broodwar->printf("Setting build order '%s'", pBuildOrder->pName);
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
			if(!Nexus->HasBase())
			{
				BWAPI::Position Position = Nexus->_Unit->getPosition();
				BWTA::BaseLocation* pBaseLocation = GetClosestBaseLocation(Position);

				if (pBaseLocation)
				{
					BWAPI::TilePosition BaseTilePosition = pBaseLocation->getTilePosition();
					Broodwar->sendText("Created base at %d, %d", BaseTilePosition.x, BaseTilePosition.y);

					Base Base = std::make_shared<CBase>(pBaseLocation);
					_Bases.push_back(Base);
					Nexus->SetBase(Base);
				}
				else
				{
					Broodwar->printf("Error: Could not find BWTA base");
				}
			}
		}
	}

	for (Base Base : _Bases)
	{
		Base->Update();
	}
}

void CTactician::UpdateScouts()
{
	if (_Me->supplyUsed() / 2 < 9)
		return;

	if (CScoutStrategy::NumScouts < 1)
	{
		for (auto It : _AgentManager._Probes)
		{
			Agent<CProbe> Probe = It.second;
			if (Probe && Probe->IsActive())
			{
				if (( !Probe->HasStrategy() || Probe->HasStrategyType<CProbe::CGatherStrategy>() ) &&
					 (!Probe->_Unit->isCarryingMinerals() && !Probe->_Unit->isCarryingGas()))
				{
					Probe->SetStrategy<CScoutStrategy>();
					break;
				}
			}
		}
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
	else if (Unit->getType() == UnitTypes::Protoss_Gateway)
	{
		RemoveAgent(_Gateways, Id);
	}
	else if (Unit->getType() == UnitTypes::Protoss_Zealot)
	{
		RemoveAgent(_Zealots, Id);
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
	else if (Unit->getType() == UnitTypes::Protoss_Gateway)
	{
		SetAgent( _Gateways, Unit );
	}
	else if (Unit->getType() == UnitTypes::Protoss_Zealot)
	{
		SetAgent( _Zealots, Unit );
	}
	else
	{
		//Broodwar->sendText("Zobbot: Could not identify agent");
	}
}

void CZobbot::onStart()
{
	// BWTA stuff
	BWTA::analyze();

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
	BWAPI::Unitset Units = Broodwar->getUnitsInRadius( GetPosition(), 10, IsResourceDepot && IsOwned && !IsBeingConstructed );
	return Units.size() > 0;
}

void CBase::Update()
{
	_MineralFields.clear();

	BWAPI::Unitset MineralFields = Broodwar->getUnitsInRadius(GetPosition(), 300, IsMineralField);
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

CBuildAction::CBuildAction(BWAPI::UnitType UnitType, const BWAPI::TilePosition& TilePosition)
	: _UnitType(UnitType)
	, _TilePosition(TilePosition)
	, _bHasLocation(true)
{
	CTactician::GetInstance()->AllocateResourcesForUnitType(_UnitType);
}

CBuildAction::CBuildAction(BWAPI::UnitType UnitType)
	: _UnitType(UnitType)
	, _bHasLocation(false)
{
	//Broodwar->sendText("Started build action '%s'", _UnitType.getName().c_str());
	CTactician::GetInstance()->AllocateResourcesForUnitType(_UnitType);
}

CBuildAction::~CBuildAction()
{
	//Broodwar->sendText("Finished build action '%s'", _UnitType.getName().c_str());
	CTactician::GetInstance()->DeallocateResourcesForUnitType(_UnitType);
}

CVision::CVision()
{
}

bool CVision::GetEnemyMain(BWAPI::TilePosition& TilePositionOut) const
{
	if (!_bFoundEnemyMain)
		return false;

	TilePositionOut = _EnemyMainLocation;

	return true;
}

bool CVision::GetUnexploredStartLocation(BWAPI::TilePosition& TilePositionOut) const
{
	if (_UnexploredStartLocations.size() < 1)
		return false;

	TilePositionOut = _UnexploredStartLocations.front();
	return true;
}

void CVision::Init()
{
	_UnexploredStartLocations = Broodwar->getStartLocations();
}

void CVision::Update()
{
	auto It = _UnexploredStartLocations.begin();
	while (It != _UnexploredStartLocations.end())
	{
		BWAPI::TilePosition TilePosition = *It;
		if (Broodwar->isVisible(TilePosition))
		{
			if (Broodwar->getUnitsOnTile(TilePosition, IsEnemy && IsResourceDepot).size() > 0)
			{
				if (!_bFoundEnemyMain)
				{
					_bFoundEnemyMain = true;
					_EnemyMainLocation = TilePosition;
					Broodwar->printf("Enemy found");
				}
			}
			else
			{
				It = _UnexploredStartLocations.erase(It);
				//Broodwar->sendText("No enemy here");
				break;
			}
		}

		++It;
	}

	if (!_bFoundEnemyMain && _UnexploredStartLocations.size() == 1)
	{
		_bFoundEnemyMain = true;
		_EnemyMainLocation = _UnexploredStartLocations.front();
		Broodwar->printf("Deduced enemy location");
	}
}
