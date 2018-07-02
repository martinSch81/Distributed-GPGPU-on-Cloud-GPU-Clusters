// Cluster distributor
// author: Schuchardt Martin, csap9442

#pragma once
#include "Node.h"
#include "Checkpoint.h"
#include "../utils/Utils.h"
#include "../utils/cl_utils.h"

#include <mpi.h>
#include <chrono>
#include <set>
#include <map>
#include <list>
#include <algorithm>
#include <sstream>
#include <queue>
#include <fstream>
#include <cassert>
#include <cmath>

// MPI process 0 on an instance uses GPU (0+ACC_DEVICE_OFFSET), process 1 uses (1+ACC_DEVICE_OFFSET), ... in case you have additional CL-devices like a CPU which you want to skip
#ifndef ACC_DEVICE_OFFSET
#define ACC_DEVICE_OFFSET 0
#endif

const char ARGUMENT_N[3] = "N=";
const char ARGUMENT_W[3] = "W=";
const char ARGUMENT_SILENT[7] = "silent";
const char ARGUMENT_MASTER_HOSTNAME[17] = "MASTER_HOSTNAME=";
const char ARGUMENT_NUM_GPUS[10] = "NUM_GPUS=";

extern const std::string MPI_HOSTFILE;
extern std::string CREATE_INSTANCES_CMD;
extern bool shutdownFlag;


class Distributor {
	friend class Checkpoint;

	int MPI_SIZE, MPI_RANK;
	int newMPI_SIZE;
	static int NUM_GPUS;
	static unsigned W; // default number of workers to start
	static bool silent;
	static std::string masterHost; // currently unused - workers get master hostname as argument
	bool _isMaster = false;
	static void setNumGPUs(unsigned num) { NUM_GPUS = num; }
	bool finished = false;
	Checkpoint checkpoint;
	std::string dotGraph;

	std::vector<std::string> rewriteArguments(const int argc, char** argv, const std::string name);

	Node *targetNode;
	std::map<std::string, Data*> parameters;
	std::set<Node*> allNodes;
	std::queue<int> idleWorkers;
	std::vector<bool> workersBusy;

	/// <summary>
	/// block additional estimations within time buffer before end of current CLUSTER_TIME_UNIT_SECONDS-block if previous estimation decided not to restart
	/// </summary>
	unsigned long long cluster_restart_block_time_unit = 0;
	/// <summary>
	/// will be set by Distributor.instanceInit() if a new cluster has been created
	/// </summary>
	std::chrono::steady_clock::time_point clusterCreation = std::chrono::steady_clock::time_point::max();
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::time_point::min();
	std::chrono::steady_clock::time_point end;

	std::stringstream output;
	std::streambuf *stdOutBkp = nullptr, *stdErrBkp = nullptr; // for silent-mode
	std::stringstream stdOutStringStream, stdErrStringStream;
	void deactivateSilentMode(std::string executable, std::string hostname);
	void sendOutput();
	void receiveOutputFromWorkers();

	/// <summary>
	/// Starting from target node, find all dependend nodes.
	/// Uses DFS, if one child appears in stacked parents, a circle has been found.
	/// 
	/// Sideffects: fills allNodes, used by Checkpoint::save, Checkpoint::read and Distributor::run so far
	/// Sideffect: writes dot-graph nodes/edges for 'graphDependencies.dot', initiated by constructor Distributor::Distributor
	///   some nodes can be visited more often than once. This cosmetic stain is externally removed from .dot file later with awk (in Makefile)
	/// </summary>
	/// <param name="current">current node</param>
	/// <param name="parent">parent node</param>
	/// <param name="checkDependencies">stack, search for circles in it</param>
	/// <returns>true if no circles have been detected</returns>
	bool findCycle(Node* child, Node* parent, std::list<Node*> &checkDependencies);

	bool restartingCluster = false;
	bool resizeInProgress() { return newMPI_SIZE != MPI_SIZE; }
	void estimateResizing(Node& n);
	bool resizeCluster(Node& n);

	int computeNodes();

	void shutdownInstances(const bool shutdown, const bool silent);
	void terminateWorker(const int w, const bool terminate = false);

	int ARGC;
	char** ARGV;
public:
	Distributor(int argc, char** argv, Node * targetNode);
	
#ifdef DEBUG_REDUCE_RESTART_CLUSTER_TIME_UNIT
	static const unsigned CLUSTER_TIME_UNIT_SECONDS = 60;
#else
	static const unsigned CLUSTER_TIME_UNIT_SECONDS = 3600;
#endif // DEBUG_REDUCE_RESTART_CLUSTER_TIME_UNIT
	static const float CLUSTER_TIME_UNIT_MAINTENANCE_BUFFER;
	static const int NO_IDLE_WORKERS_AVAILABLE = -1;

#ifdef DEBUG_FORCE_ALWAYS_USING_ACC_DEVICE_0
	unsigned assignedGPU_Device() { return 0 + ACC_DEVICE_OFFSET; }
#else
	unsigned assignedGPU_Device() { return MPI_RANK % NUM_GPUS + ACC_DEVICE_OFFSET; }
#endif // DEBUG_FORCE_ALWAYS_USING_ACC_DEVICE_0

	std::string deviceInfoCL();
	static int getNumGPUs() { return NUM_GPUS; }
	unsigned getW() { return W; }

	/// <summary>
	/// Simple kernel for master and workers, sends shutdown signal to workers and collects their output in Distributor::instanceFinalize. Use like any normal kernel.
	/// </summary>
	/// <param name="n">Node (automatically set by Distributor)</param>
	/// <param name="distributor">Distributor (automatically set by Distributor)</param>
	/// <returns>0</returns>
	static int kernel_shutdown(Node & n, Distributor & distributor);

	bool isMaster() { return _isMaster; }
	unsigned getSize() { return static_cast<unsigned>(MPI_SIZE); }
	unsigned getRank() { return static_cast<unsigned>(MPI_RANK); }
	int getRootID();
	bool isFinished() { return finished; }

	void addArgument(const std::string &key, Data* value) { parameters[key] = value; }
	void addArguments(const std::vector<std::pair<std::string, Data*>> datas) {
		for (auto d : datas)
			addArgument(d.first, d.second);
	}
	std::map<std::string, Data*> &getArguments() { return parameters; }
	Data* getArgument(const std::string &key) { return parameters.at(key); }

	const long long getDuration();
	std::vector<std::pair<std::string, unsigned long long>> getDurationDetails();

	std::string getOutput();
	void addOutput(std::string line) { output << line; }
	std::string whoAmI();


	/// <summary>
	/// Initializes cluster, distributes binary and starts executable on all instances. 
	/// If no cluster exists (no file MPI_HOSTFILE exists), cmd CREATE_INSTANCES_CMD is executed with param W for requested number of instances. 
	/// Sets MPI_COMM_WORKER_TO_WORKER and MPI_COMM_CLUSTER. 
	/// Attempt, to read checkpoint if exists.
	/// </summary>
	/// <param name="argc">argc from main</param>
	/// <param name="argv">argv from main</param>
	/// <param name="_W">number of workers to create when launching a new cluster</param>
	/// <param name="_silent">pipe output to .out/.err or stdout/stderr</param>
	/// <returns>0 if successful</returns>
	int instanceInit();

	/// <summary>
	/// Sets target node on that later Distributor::run may operate. All dependencies (recursive) of this node need to be computed before this node can be computed.
	/// 
	/// Sideffect: calls findCycle, which generates dependency graph and fills set allNodes.
	/// </summary>
	/// <param name="targetNode">Node with the final computation. This node depends on all preceding computations.</param>
	/// <returns>true if target node's graph does not contain circles.</returns>
	bool setTargetNode(Node *targetNode);
	
	/// <summary>
	/// Solve graph, bottom up with all dependencies of target node.
	/// 
	/// Sideffect: generates a dot-graph (graphComputation.dot) to document when/which nodes have been computed. Nodes can add details (worker-chunks) to graph. Nodes might initiate restart. If so, node has written a checkpoint, run exits, main restarts, next run will know from finishedNodeIDs which nodes have already been computed.
	/// </summary>
	/// <returns>RC=0 if no error occured</returns>
	int run();

	int nextWorker(Node* n);
	/// <summary>
	/// Marks worker-id for distributor as idle. Sideffect: if this nodes supports cluster restart, it will estimated. Check distributor.isRestarting() after call. Sideffect: adds worker/chunk (LOOP_COUNTER) to node's dotGraph.
	/// </summary>
	/// <param name="n">Current node to get additional information regarding cluster restart and node's dotGraph.</param>
	/// <param name="worker">id of now idle worker</param>
	/// <returns></returns>
	void addToIdleQueue(Node& n, int worker);
	std::size_t availableWorkers() { return idleWorkers.size(); }

	bool resizeCluster(Node& n, const int _newSize);
	bool isRestarting() { return restartingCluster; }

	Checkpoint& getCheckpoint() { return checkpoint; }
	bool saveCheckpoint(Node* n);
	bool readCheckpoint() { return checkpoint.read(this); }

	void graphAppendNode(const std::string id, const std::string description, const bool masterOnlyNode);
	void graphAppendNode(Node* n);
	void graphAppendEdge(Node* n);
	void graphAppendEdge(const std::string from, const std::string to);
	std::set<Node*> getSuccessors(Node *n);

	void instanceFinalize();
	/// <summary>
	/// shutdown flag: true=shutdown, false=log shutdown attempt only (=default)
	/// </summary>
	/// <param name="_shutdown"></param>
	void setShutdownFlag(const bool _shutdown) { shutdownFlag = _shutdown; }
	
	void terminateWorkers();
	void expectTerminate();
};
