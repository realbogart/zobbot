#pragma once
#include <BWAPI.h>
#include <vector>

#include "buildorders.h"

struct SCommand
{
	float _Score = 0.0f;
};

class CAgentStrategyBase;
class CTactician;

class CAgent
{
public:
	CAgent(const BWAPI::Unit Unit, CAgentStrategyBase* pAgentStrategy = nullptr)
		: _Unit( Unit )
		, _pAgentStrategy( pAgentStrategy )
	{
	}

	virtual ~CAgent() {}

	void DoAction();

	bool IsActive();

	template<typename T, typename... Args>
	void SetStrategy(Args... args)
	{
		BWAPI::Broodwar->sendText("Assigning strategy '%s' to id = %d", typeid(T).name(), _Unit->getID());
		
		if (_pAgentStrategy)
		{
			ResetStrategy();
		}

		_pAgentStrategy = new T(args...);
	}

	void ResetStrategy();

	bool HasStrategy() { return _pAgentStrategy != nullptr; }

	template<typename T>
	T* AccessStrategy() { return dynamic_cast<T*>(_pAgentStrategy); }

	template<typename T>
	bool HasStrategyType() { return AccessStrategy<T>() != nullptr; }

	BWAPI::Unit _Unit = nullptr;

protected:
	CAgentStrategyBase* _pAgentStrategy = nullptr;
};

class CAgentStrategyBase
{
public:
	virtual ~CAgentStrategyBase() {};
	virtual void DoAction(CAgent* pAgent) = 0;
};

template<typename T>
class CAgentStrategy : public CAgentStrategyBase
{
public:
	void DoAction(CAgent* pAgent) override
	{
		T* pAgentTyped = dynamic_cast<T*>(pAgent);

		if (pAgentTyped)
		{
			Do(pAgentTyped);
		}
	}

	virtual void Do(T* pAgent) = 0;
};

class CBuildAction
{
public:
	CBuildAction(BWAPI::UnitType UnitType);
	~CBuildAction();

	BWAPI::UnitType GetUnitType() { return _UnitType; }

	void SetCompleted(bool bCompleted) { _bCompleted = bCompleted; }
	bool IsCompleted() { return _bCompleted; }

private:
	BWAPI::UnitType _UnitType;
	bool _bCompleted = false;
};

using BuildAction = std::shared_ptr<CBuildAction>;

class CBase
{
public:
	CBase(BWAPI::Position NexusPosition)
		: _NexusPosition(NexusPosition)
	{
	}

	bool HasNexus();
	void Update();

	BWAPI::Unit GetMineralField();
	bool HasMineralField();

	BWAPI::Position GetPosition() { return _NexusPosition; }

private:
	std::vector<BWAPI::Unit> _MineralFields;
	int _CurrentMineralField = 0;

	BWAPI::Position _NexusPosition;
};

using Base = std::shared_ptr<CBase>;

class CProbe : public CAgent
{
public:
	class CGatherStrategy : public CAgentStrategy<CProbe>
	{
	public:
		CGatherStrategy(Base Base);
		void Do(CProbe* pProbe);

	private:
		Base _Base;
	};

	class CBuildStrategy : public CAgentStrategy<CProbe>
	{
	public:
		CBuildStrategy(BuildAction BuildAction);

		void Do(CProbe* pProbe);

		BuildAction AccessBuildAction() { return _BuildAction; }

	private:
		BuildAction _BuildAction;
		bool _bStartedBuildAction = false;
	};

	CProbe(const BWAPI::Unit Unit)
		: CAgent( Unit )
	{
	}
};

class CNexus : public CAgent
{
public:
	class CBuildProbe : public CAgentStrategy<CNexus>
	{
	public:
		void Do(CNexus* pNexus);
	};

	CNexus(const BWAPI::Unit Unit)
		: CAgent(Unit)
	{
		SetStrategy<CBuildProbe>();
	}

	void SetBase(Base Base) { _Base = Base; }
	bool HasBase() { return _Base != nullptr; }
	Base GetBase() { return _Base; }

private:
	Base _Base;
};

template<typename T>
using Agent = std::shared_ptr<T>;

template<typename T>
using AgentMap = std::unordered_map< int, Agent<T> >;

class CAgentManager
{
public:
	void RefreshAgents();
	void DoActions();

	void RemoveAgent(const BWAPI::Unit Unit);

	template<typename T>
	static Agent<T> AccessClosest(AgentMap<T>& Map, const BWAPI::Position& Position)
	{
		int Max = INT_MAX;
		Agent<T> ClosestAgent = nullptr;
		for (auto It : Map)
		{
			Agent<T> Current = std::dynamic_pointer_cast<T>( It.second );
			if (Current && Current->IsActive())
			{
				int Distance = Current->_Unit->getDistance(Position);
				if (Distance < Max)
				{
					Max = Distance;
					ClosestAgent = Current;
				}
			}
		}

		return ClosestAgent;
	}

	AgentMap<CAgent> _AllAgents;
	AgentMap<CProbe> _Probes;
	AgentMap<CNexus> _Nexuses;

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
		
		Agent<T> Agent = std::make_shared<T>(Unit);
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
};

class CTactician
{
public:
	void OnStart();
	void Update();

	CAgentManager& GetAgentManager() { return _AgentManager; }

	void AllocateResourcesForUnitType(BWAPI::UnitType UnitType);
	void DeallocateResourcesForUnitType(BWAPI::UnitType UnitType);

	void AllocateResources(int Minerals, int Gas) { _AllocatedMinerals += Minerals;  _AllocatedGas += Gas; }
	void DeallocateResources(int Minerals, int Gas) { _AllocatedMinerals -= Minerals; _AllocatedGas -= Gas; };

	int GetUnallocatedMinerals();
	int GetUnallocatedGas();

	void GetResources(int& MineralsOut, int& GasOut);
	void GetUnallocatedResources(int& MineralsOut, int& GasOut);

	bool CanAfford(BWAPI::UnitType UnitType);
	bool CanAffordUnallocated(BWAPI::UnitType UnitType);

	void UnitCreated(BWAPI::Unit Unit);

	static CTactician* CreateInstance() { _pInstance = new CTactician(); return _pInstance; };
	static CTactician* GetInstance() { return _pInstance; };

private:
	const SBuildOrder* _pBuildOrder = nullptr;

	static CTactician* _pInstance;
	Agent<CProbe> GetBuilder();

	void SetBuildOrder(const char* pName);
	void UpdateBases();
	void SetProbeStrategies();
	void UpdateBuildActions();
	void AddBuildAction(BWAPI::UnitType UnitType);
	bool NeedPylon();
	int CountBuildActionFor(BWAPI::UnitType UnitType);
	bool HasBuildActionFor(BWAPI::UnitType UnitType);
	int GetUnitTypeCount(BWAPI::UnitType UnitType, bool bCountUnfinished);

	int _AllocatedMinerals = 0;
	int _AllocatedGas = 0;

	BWAPI::Player _Me;

	CAgentManager _AgentManager;

	std::vector<Base> _Bases;
	std::vector<BuildAction> _BuildActions;
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
};
