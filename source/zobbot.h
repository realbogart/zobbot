#pragma once
#include <BWAPI.h>
#include <vector>

struct SCommand
{
	float _Score = 0.0f;
};

class CAgent
{
	friend class CAgentManager;

public:
	CAgent(const BWAPI::Unit Unit)
		: _Unit( Unit )
	{
	}

	virtual void DoAction() = 0;

	bool IsActive();

protected:
	BWAPI::Unit _Unit = nullptr;
};

class CProbe : public CAgent
{
public:
	CProbe(const BWAPI::Unit Unit)
		: CAgent( Unit )
	{
	}

	void DoAction() override;
};

class CNexus : public CAgent
{
public:
	CNexus(const BWAPI::Unit Unit)
		: CAgent(Unit)
	{
	}

	void DoAction() override;
};

template<typename T>
using AgentMap = std::unordered_map< int, std::shared_ptr<T> >;

class CAgentManager
{
public:
	void RefreshAgents();
	void DoActions();

	void RemoveAgent(const BWAPI::Unit Unit);

private:
	template<typename T>
	void SetAgent(AgentMap<T>& AgentMap, const BWAPI::Unit& Unit)
	{
		int Id = Unit->getID();

		auto It = _AllAgents.find(Id);
		if (It != _AllAgents.end())
		{
			It->second->_Unit = Unit;
			return;
		}

		//Broodwar->sendText("Adding new agent with Id = %d", Id);
		
		std::shared_ptr<T> Agent = std::make_shared<T>(Unit);
		AgentMap.emplace(Id, Agent);
		_AllAgents.emplace(Id, Agent);
	}

	template<typename T>
	void RemoveAgent(AgentMap<T>& AgentMap, int Id)
	{
		auto It = AgentMap.find(Id);
		if (It != AgentMap.end())
			AgentMap.erase(It);
	}

	void RefreshAgent(const BWAPI::Unit Unit);

	AgentMap<CAgent> _AllAgents;
	AgentMap<CProbe> _Workers;
	AgentMap<CNexus> _Nexuses;
};

class CTactician
{
public:
	void Update();

	CAgentManager& GetAgentManager() { return _AgentManager; }

private:
	CAgentManager _AgentManager;
};

class CZobbot : public BWAPI::AIModule
{
public:
	virtual void onStart();
	virtual void onEnd(bool isWinner);
	virtual void onFrame();
	virtual void onSendText(std::string text);
	virtual void onReceiveText(BWAPI::Player player, std::string text);
	virtual void onPlayerLeft(BWAPI::Player player);
	virtual void onNukeDetect(BWAPI::Position target);
	virtual void onUnitDiscover(BWAPI::Unit unit);
	virtual void onUnitEvade(BWAPI::Unit unit);
	virtual void onUnitShow(BWAPI::Unit unit);
	virtual void onUnitHide(BWAPI::Unit unit);
	virtual void onUnitCreate(BWAPI::Unit unit);
	virtual void onUnitDestroy(BWAPI::Unit unit);
	virtual void onUnitMorph(BWAPI::Unit unit);
	virtual void onUnitRenegade(BWAPI::Unit unit);
	virtual void onSaveGame(std::string gameName);
	virtual void onUnitComplete(BWAPI::Unit unit);

private:
	CTactician _Tactician;
};
