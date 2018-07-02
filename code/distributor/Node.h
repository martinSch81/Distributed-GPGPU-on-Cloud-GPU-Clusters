// node in work-graph
// author: Schuchardt Martin, csap9442

#pragma once

#include <map>
#include <list>
#include <string>
#include <sstream>
#include <chrono>

#include "Data.h"



class Node;
class Distributor;
typedef int(*krnl)(Node&, Distributor&);


class Node {
	friend class Checkpoint;
	friend class Distributor;

	static unsigned nodeCount;
	unsigned id;
	const std::string description;
	krnl masterKernel, workerKernel;

	std::map<std::string, Data*> parameters;
	std::list<Node*> dependencies;

	std::stringstream output;

	bool finished = false;
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::time_point::min(); // first execution of node (may be interrupted by restart)
	std::chrono::steady_clock::time_point lastStart; // current start of execution (resetted after restart - required to estimate current time per chunk/node)
	std::chrono::steady_clock::time_point end;

	unsigned LOOP_COUNTER = 0;
	unsigned TOTAL_COUNT = 0;
	unsigned LOOP_COUNTER_lastStart = 0;

	std::map<int, int> workers_dotGraph; // to remember predecessor .dot-node in .dot-graph for individual workers
	std::string dotGraph_predecessor;
	void dotGraph_WorkerUnionBeforeClusterRestart(Distributor* distributor);
	void dotGraph_WorkerUnionAfterCompletion(Distributor* distributor);
	void dotGraph_AppendWorkerChunk(const int worker, Distributor* distributor);

public:

	Node(std::string description, krnl mk, krnl wk = nullptr) : id(nodeCount++), description(description), masterKernel(mk), workerKernel(wk) {}
	unsigned getID() { return id; }
	const std::string & getDescription() { return description; }
	const bool hasFinished() { return finished; }
	void hasFinished(bool hasFinished) { finished = hasFinished; }
	const bool isReady();
	bool getIsMasterOnly() { return !workerKernel; }

	void addArgument(const std::string &key, Data* value) { parameters[key] = value; }
	void addArguments(const std::vector<std::pair<std::string, Data*>> datas) {
		for (auto d : datas)
			addArgument(d.first, d.second);
	}
	std::map<std::string, Data*> &getArguments() { return parameters; }
	Data* getArgument(const std::string &key) { return parameters.at(key); }
	std::list<Node*> &getDependencies() { return dependencies; }
	void addDependency(Node *n) { dependencies.push_back(n); }

	std::string logline(std::string line, std::string whoAmI);
	std::string getOutput() { return output.str(); }
	void addOutput(std::string line) { output << line; }
	void clearOutput() { 
		output.str(std::string()); 
		output.clear();
	}

	int run(Distributor &distributor);
	const long long getDuration();
	std::pair<std::string, long long unsigned> getDurationDetails();

	unsigned* const getLoopCounter() { return &LOOP_COUNTER; }
	const bool isResizeable() { return TOTAL_COUNT > LOOP_COUNTER; }
	void activateAutoResize(unsigned _total_count) { TOTAL_COUNT = _total_count; }
};
