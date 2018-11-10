#include "zobbot.h"
#include <iostream>

using namespace BWAPI;
using namespace Filter;

void CAgent::DoAction()
{
	if (_AgentStrategy)
	{
		_AgentStrategy->DoAction(this);
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

				//if (!Unit->gather(Unit->getClosestUnit(IsMineralField || IsRefinery)))
				//{
				//	Broodwar << Broodwar->getLastError() << std::endl;
				//}
			}
		}
	}
}

void CNexus::CBuildProbe::Do(CNexus* pNexus)
{
	BWAPI::Unit Unit = pNexus->_Unit;
	if (Unit->isIdle() && !Unit->train(Unit->getType().getRace().getWorker()))
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

void CTactician::Update()
{
	_AgentManager.RefreshAgents();

	UpdateBases();
	SetProbeStrategies();

	_AgentManager.DoActions();
}

void CTactician::OnStart()
{
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

	_Tactician.OnStart();
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

	_Tactician.Update();
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

void CZobbot::onUnitCreate(BWAPI::Unit unit)
{
	if (Broodwar->isReplay())
	{
		if (unit->getType().isBuilding() && !unit->getPlayer()->isNeutral())
		{
			int seconds = Broodwar->getFrameCount() / 24;
			int minutes = seconds / 60;
			seconds %= 60;
			Broodwar->sendText("%.2d:%.2d: %s creates a %s", minutes, seconds, unit->getPlayer()->getName().c_str(), unit->getType().c_str());
		}
	}
}

void CZobbot::onUnitDestroy(BWAPI::Unit Unit)
{
	_Tactician.GetAgentManager().RemoveAgent( Unit );
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

	BWAPI::Unitset MineralFields = Broodwar->getUnitsInRadius(_NexusPosition, 500, IsMineralField);
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
