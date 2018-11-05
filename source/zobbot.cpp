#include "zobbot.h"
#include <iostream>

using namespace BWAPI;
using namespace Filter;

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

void CProbe::DoAction()
{

	if (_Unit->isIdle() )
	{
		if (_Unit->isCarryingGas() || _Unit->isCarryingMinerals())
		{
			_Unit->returnCargo();
		}
		else
		{
			if (!_Unit->gather(_Unit->getClosestUnit(IsMineralField || IsRefinery)))
			{
				Broodwar << Broodwar->getLastError() << std::endl;
			}
		}
	}
}

void CNexus::DoAction()
{
	if (_Unit->isIdle() && !_Unit->train(_Unit->getType().getRace().getWorker()))
	{
		Position pos = _Unit->getPosition();
		Error lastErr = Broodwar->getLastError();
		Broodwar->registerEvent([pos, lastErr](Game*) { Broodwar->drawTextMap(pos, "%c%s", Text::White, lastErr.c_str()); }, nullptr, Broodwar->getLatencyFrames());
	
		UnitType supplyProviderType = _Unit->getType().getRace().getSupplyProvider();
		static int lastChecked = 0;
	
		if (lastErr == Errors::Insufficient_Supply &&
			lastChecked + 400 < Broodwar->getFrameCount() &&
			Broodwar->self()->incompleteUnitCount(supplyProviderType) == 0)
		{
			lastChecked = Broodwar->getFrameCount();
	
			Unit supplyBuilder = _Unit->getClosestUnit(GetType == supplyProviderType.whatBuilds().first &&
				(IsIdle || IsGatheringMinerals) &&
				IsOwned);
	
			if (supplyBuilder)
			{
				if (supplyProviderType.isBuilding())
				{
					TilePosition targetBuildLocation = Broodwar->getBuildLocation(supplyProviderType, supplyBuilder->getTilePosition());
					if (targetBuildLocation)
					{
						Broodwar->registerEvent([targetBuildLocation, supplyProviderType](Game*)
						{
							Broodwar->drawBoxMap(Position(targetBuildLocation), Position(targetBuildLocation + supplyProviderType.tileSize()), Colors::Blue);
						}, nullptr, supplyProviderType.buildTime() + 100);
	
						supplyBuilder->build(supplyProviderType, targetBuildLocation);
					}
				}
				else
				{
					supplyBuilder->train(supplyProviderType);
				}
			}
		}
	}
}

void CTactician::Update()
{
	_AgentManager.RefreshAgents();
	_AgentManager.DoActions();
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
		RemoveAgent(_Workers, Id);
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
		SetAgent( _Workers, Unit );
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
