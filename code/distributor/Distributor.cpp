// Cluster distributor
// author: Schuchardt Martin, csap9442

#include "Distributor.h"

MPI_Comm MPI_COMM_CLUSTER;
MPI_Comm MPI_COMM_WORKER_TO_WORKER;
int DISTRIBUTOR_ROOT_NODE;
int DISTRIBUTOR_MPI_SIZE;
std::string CREATE_INSTANCES_CMD("./createInstances.sh");
const std::string MPI_HOSTFILE("mpi.hostfile");

int Distributor::NUM_GPUS = 0;
unsigned Distributor::W = 2; // default number of workers to start
bool Distributor::silent = false;
std::string Distributor::masterHost;

bool shutdownFlag = false;

#ifdef DEBUG_REDUCE_RESTART_CLUSTER_TIME_UNIT
const float Distributor::CLUSTER_TIME_UNIT_MAINTENANCE_BUFFER = 0.75f;
#else
const float Distributor::CLUSTER_TIME_UNIT_MAINTENANCE_BUFFER = 0.9f;
#endif // DEBUG_REDUCE_RESTART_CLUSTER_TIME_UNIT


const char DOT_GRAPH_HEADER[] = "digraph graphname{\n  graph [ ranksep=\"0.2\", nodesep=\"0.08\"];\n";
const char DOT_GRAPH_FOOTER[] = "}";


int Distributor::instanceInit() {
	int err = MPI_Init(&ARGC, &ARGV);
	if (err == MPI_SUCCESS) {
		err = MPI_Comm_size(MPI_COMM_WORLD, &MPI_SIZE);
		err |= MPI_Comm_rank(MPI_COMM_WORLD, &MPI_RANK);
	}
	MPI_Comm_get_parent(&MPI_COMM_CLUSTER);
	storeMPIInstanceName();

	std::string arg;
	if (parseArguments(ARGC, ARGV, ARGUMENT_SILENT, arg) >= 0) {
		Distributor::silent = true;
		if (parseArguments(ARGC, ARGV, ARGUMENT_MASTER_HOSTNAME, arg) >= 0) {
			Distributor::masterHost = arg;
			assert(masterHost != "");
		}
	}
	if (parseArguments(ARGC, ARGV, ARGUMENT_W, arg) >= 0)
		Distributor::W = atoi(arg.c_str());
	if (parseArguments(ARGC, ARGV, ARGUMENT_NUM_GPUS, arg) >= 0)
		Distributor::setNumGPUs(atoi(arg.c_str()));

	if (MPI_COMM_CLUSTER == MPI_COMM_NULL) {
		_isMaster = true;
		// if no mpi.hostfile exists, call the script to create W instances
		std::ifstream f(MPI_HOSTFILE, std::ios_base::in);
		if (!f.good()) {
			f.close();
			clusterCreation = std::chrono::steady_clock::now();
			err = system((CREATE_INSTANCES_CMD + ' ' + std::to_string(Distributor::W)).c_str());
			addOutput("  " + nowToString() + ": " + whoAmI() + ": launched new cluster:\n");
			if (err != 0) {
				std::cerr << "ERROR: could not generate instances (rc=" + std::to_string(err) +"), terminating now..." << std::endl;
				return err;
			}
		} else {
			f.close();
			addOutput("  " + nowToString() + ": " + whoAmI() + ": using an existing cluster:\n");
		}

		bool masterComputes;
		auto universe_size = splitHostfile(MPI_HOSTFILE, &NUM_GPUS, nullptr, nullptr, &masterComputes);
		if (universe_size < 1) {
			std::cerr << "ERROR: invalid " + MPI_HOSTFILE + ". Return code " + std::to_string(universe_size) << std::endl;
			std::cerr << MPI_HOSTFILE_USAGE << std::endl;
			return -1;
		}
		addOutput(indentLogText(std::to_string(universe_size) + " workers to launch, " + std::to_string(NUM_GPUS) + " GPUs per instance\n"));
		if (masterComputes)
			addOutput(indentLogText(std::to_string(NUM_GPUS) + " of the workers will be launchend on master\n"));


		std::string executableBaseName = getExecutableBaseName(std::string(ARGV[0]));
		if (executableBaseName == "") {
			std::cerr << "ERROR: could not parse executable name from argv: '" + std::string(ARGV[0]) + "', terminating now..." << std::endl;
			return -1;
		}
		std::string distributeCMD = executableBaseName.insert(0, "make distribute ALLEXECUTABLES=");
		err = system(distributeCMD.c_str());
		if (err != 0) {
			std::cerr << "ERROR: could not distribute executable to cluster: '" + distributeCMD + "', terminating now..." << std::endl;
			return err;
		}

		MPI_Info info;
		MPI_Info_create(&info);
		auto rc = MPI_Info_set(info, const_cast<char*>("wdir"), const_cast<char*>(WORKDIR));
		rc |= MPI_Info_set(info, const_cast<char*>("add-hostfile"), const_cast<char*>(MPI_HOSTFILE.c_str()));
		rc |= MPI_Info_set(info, const_cast<char*>("npernode"), const_cast<char*>(std::to_string(NUM_GPUS).c_str()));
		rc |= MPI_Info_set(info, const_cast<char*>("bind_to"), const_cast<char*>("none:overload-allowed"));
		assert(rc == 0);

		std::vector<char*> cstrings;
		auto strings = rewriteArguments(ARGC, ARGV, getName());
		for (size_t i = 0; i < strings.size(); ++i)
			cstrings.push_back(const_cast<char*>(strings[i].c_str()));
		cstrings.push_back('\0');
		err |= MPI_Comm_spawn(const_cast<char*>(getExecutableFullName(ARGV[0]).c_str()), &cstrings[0], universe_size, info, 0, MPI_COMM_SELF, &MPI_COMM_CLUSTER, MPI_ERRCODES_IGNORE);
		if (err != 0) {
			std::cerr << "ERROR: could not spawn MPI processes (rc=" + std::to_string(err) + "), terminating now..." << std::endl;
			return err;
		}

		MPI_Comm_remote_size(MPI_COMM_CLUSTER, &universe_size);
		newMPI_SIZE = MPI_SIZE = universe_size;

		std::queue<int>().swap(idleWorkers); // clear queue in case it was already in use (eg. this function is called during cluster resize)
		for (int i = 0; i < universe_size; ++i)
			idleWorkers.push(i);
			
		workersBusy.resize(universe_size);
		for (unsigned i = 0; i < workersBusy.size(); ++i)
			workersBusy[i] = false;

		MPI_COMM_WORKER_TO_WORKER = MPI_COMM_NULL;
	} else {
		_isMaster = false;

		if (Distributor::NUM_GPUS <= 0) {
			std::cerr << "ERROR: number of GPU-devices is " + std::to_string(Distributor::NUM_GPUS) + ", but needs to be greater than 0" << std::endl;
			exit(EXIT_FAILURE);
		}

		MPI_COMM_WORKER_TO_WORKER = MPI_COMM_WORLD;
	}

	DISTRIBUTOR_ROOT_NODE = getRootID(); // used in Data-class
	DISTRIBUTOR_MPI_SIZE = getSize(); // used in Data-class 

	if (!readCheckpoint()) {
		std::cerr << "ERROR: could not read checkpoint (" + CHECKPOINT_FILE + "). Terminating now." << std::endl;
		exit(EXIT_FAILURE);
	}

	if (Distributor::silent) {
#ifndef DEBUG_IGNORE_SILENT_STDOUT
		stdOutBkp = std::cout.rdbuf(stdOutStringStream.rdbuf());
#endif
#ifndef DEBUG_IGNORE_SILENT_STDERR
			stdErrBkp = std::cerr.rdbuf(stdErrStringStream.rdbuf());
#endif
	}

	return err;
}

void Distributor::graphAppendNode(Node* n) {
	graphAppendNode(std::to_string(n->getID()), n->getDescription(), n->getIsMasterOnly());
}

void Distributor::graphAppendNode(const std::string id, const std::string description, const bool masterOnlyNode) {
	std::string color("black");
	if (masterOnlyNode)
		color = "blue";
	dotGraph.append(id + " [label=\"" + description + "\",color=" + color + "];\n");
}

void Distributor::graphAppendEdge(Node* n) {
	for (auto dep : n->getDependencies())
		graphAppendEdge(std::to_string(dep->getID()), std::to_string(n->getID()));
}

void Distributor::graphAppendEdge(const std::string from, const std::string to) {
	dotGraph.append(from + " -> " + to + " [arrowsize=.5, weight=2.]\n");
}

std::set<Node*> Distributor::getSuccessors(Node *n) {
	std::set<Node*> result;
	std::queue<Node*> search;
	std::set<Node*> visited;
	
	search.push(targetNode);
	while (search.size() > 0) {
		auto node = search.front();
		search.pop();
		
		visited.insert(node);
		for (auto dep : node->getDependencies()) {
			if (dep->getID() == n->getID())
				result.insert(node);
			
			if (visited.find(dep) == visited.end())
				search.push(dep);
		}
	}

	return result;
}

int Distributor::computeNodes() {
	if (start == std::chrono::steady_clock::time_point::min()) // initial value, if not overwritten by checkpoint
		start = std::chrono::steady_clock::now();

	int err = 0;

	if (dotGraph.size() == 0) // initialize, if not prefilled from cluster restart
		dotGraph.assign(DOT_GRAPH_HEADER);

	// start working on target node bottom up through all dependencies
	std::list<Node*> readySet, dependenciesSet;
	dependenciesSet.insert(dependenciesSet.end(), allNodes.begin(), allNodes.end());

	while (dependenciesSet.size() != 0 || readySet.size() != 0) {
		std::list<Node*> nextNodes;
		for (auto n = dependenciesSet.begin(); n != dependenciesSet.end(); ++n)
			if ((*n)->isReady())
				nextNodes.push_back(*n);

		for (auto n : nextNodes) {
			readySet.push_back(n);
			dependenciesSet.erase(std::find(dependenciesSet.begin(), dependenciesSet.end(), n));
		}

		std::list<Node*> computedNodes;
		for (auto n : readySet) {
			if (!n->hasFinished()) {
				graphAppendNode(n);
				err |= n->run(*this);
				if (restartingCluster) {
					return err;
				}

				output << n->getOutput();
				graphAppendEdge(n);
			}

			computedNodes.push_back(n);
		}

		for (auto n : computedNodes)
			readySet.erase(std::find(readySet.begin(), readySet.end(), n));
	}
	finished = true;


	if (restartingCluster)
		return err;

	end = std::chrono::steady_clock::now();

	dotGraph.append(DOT_GRAPH_FOOTER);
	std::ofstream dotFile;
	dotFile.open("graphComputation.dot", std::ios::out | std::ios::trunc);
	dotFile << dotGraph;
	dotFile.close();

	return err;

}

/// <summary>
/// run() includes computing all dependend nodes of the graph, then finalizing the cluster again (collecting logs, shutdown instances (if flag is set))
/// </summary>
/// <returns></returns>
int Distributor::run() {
	int err = computeNodes();
	instanceFinalize();
	return err;
}

bool Distributor::findCycle(Node* current, Node* parent, std::list<Node*> &checkDependencies) {
	auto collission = std::find(checkDependencies.cbegin(), checkDependencies.cend(), current);
	if (collission != checkDependencies.cend()) {
		graphAppendEdge(std::to_string((*collission)->getID()), std::to_string(parent->getID()));
		return false;
	}

	allNodes.insert(current);
	graphAppendNode(current);
	if (parent)
		graphAppendEdge(std::to_string(current->getID()), std::to_string(parent->getID()));

	if (current->getDependencies().size() == 0) {
		return true;
	} else {
		checkDependencies.push_back(current);
		for (auto child : current->getDependencies()) {
			if (!findCycle(child, current, checkDependencies))
				return false;
		}
		checkDependencies.pop_back();
	}
	return true;
}

bool Distributor::setTargetNode(Node * _targetNode) {
	targetNode = _targetNode;

	allNodes.clear();

	// backup original dotGraph (could have content if we are continuing after a restart)
	// then use graph functions to draw a dependency graph
	std::string bkpDotGraph = dotGraph;
	dotGraph.assign(DOT_GRAPH_HEADER);

	std::list<Node*> checkDependencies;
	const bool result = findCycle(targetNode, nullptr, checkDependencies);

	dotGraph.append(DOT_GRAPH_FOOTER);
	std::ofstream dotFile;
	dotFile.open("graphDependencies.dot", std::ios::out | std::ios::trunc);
	dotFile << dotGraph;
	dotFile.close();

	dotGraph = bkpDotGraph;

	return result;
}

int Distributor::getRootID() {
	if (_isMaster)
		return MPI_ROOT;
	return 0;
}

const long long Distributor::getDuration() {
	long long elapsed_ms = 0;
	if (finished)
		elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	else
		elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

	return elapsed_ms;
}

std::vector<std::pair<std::string, unsigned long long>> Distributor::getDurationDetails() {
	std::vector<std::pair<std::string, unsigned long long>> result;
	for (auto n : allNodes)
		result.push_back(n->getDurationDetails());

	if (!finished)
		return result;

	for (auto subResult : Data::getDurationDetails())
		result.push_back(subResult);
	
	return result;
}


std::string Distributor::getOutput() {
	std::string name(whoAmI());
	return std::string(10, '*') + " " + name + " " + std::string(70 - name.size() - 2, '*') + '\n' + output.str() + std::string(80, '*');
}

std::string Distributor::whoAmI() {
	std::string name = getName();

	std::string result = isMaster() ? "Master(" + name + ")" : "Worker(" + std::to_string(getRank()) + ", " + name + ")";
	return result;
}

int Distributor::nextWorker(Node* n) {
	if (isRestarting()) // node has to exit and must not try to retrieve workers once a restart has been initiated
		n->addOutput(indentLogText("distributor refused attempt to get an idle worker because a restart is in progress..."));

	if (idleWorkers.empty())
		return Distributor::NO_IDLE_WORKERS_AVAILABLE;

	auto result = idleWorkers.front();
	workersBusy[result] = true;

	idleWorkers.pop();
	return result;
}

void Distributor::addToIdleQueue(Node& n, int worker) {
	if (!resizeInProgress() || !n.isResizeable())
			idleWorkers.push(worker);

	
	workersBusy[worker] = false;

	n.dotGraph_AppendWorkerChunk(worker, this);
	estimateResizing(n);

	if (resizeInProgress())
		resizeCluster(n);
}

bool Distributor::resizeCluster(Node& n, const int _newSize) {
	if (MPI_SIZE == _newSize)
		return true;

	newMPI_SIZE = _newSize;
	return resizeCluster(n);
}

bool Distributor::resizeCluster(Node& n) {
	if (!resizeInProgress() || !n.isResizeable())
		return true;
	
	static bool once = true;
	if (once) {
		n.addOutput("  " + nowToString() + ": " + whoAmI() + ": Initiating cluster resize from " + std::to_string(MPI_SIZE) + " to " + std::to_string(newMPI_SIZE) + '\n');
		once = false;
	}
	for (int i = 0; i < MPI_SIZE; ++i)
		if (workersBusy[i] == true)
			return false;
	
	splitHostfile(MPI_HOSTFILE, nullptr, nullptr, &newMPI_SIZE, nullptr);

	for (int i = newMPI_SIZE; i < MPI_SIZE; ++i) {
		terminateWorker(i, true); // terminate all nodes that won't be used any more
	}
	for (int i = 0; i < newMPI_SIZE; ++i) {
		terminateWorker(i, false); // just stop current MPI process on nodes that will be reused
	}
	n.addOutput("  " + nowToString() + ": " + whoAmI() + ": Cluster resized from " + std::to_string(MPI_SIZE) + " to " + std::to_string(newMPI_SIZE) +'\n');
	MPI_SIZE = newMPI_SIZE;
	restartingCluster = true;

	std::queue<int> empty;
	std::swap(idleWorkers, empty);

	once = true;
	n.addOutput("  " + nowToString() + ": " + whoAmI() + ": restarting cluster now...\n");

	n.dotGraph_WorkerUnionBeforeClusterRestart(this);
	if (!saveCheckpoint(&n)) {
		std::cerr << "ERROR: could not write checkpoint (" + CHECKPOINT_FILE + "). Terminating now." << std::endl;
		exit(EXIT_FAILURE);
	}

	
	return true;
}

void Distributor::terminateWorkers() {
	if (isMaster())
		for (int i = 0; i < MPI_SIZE; ++i)
			terminateWorker(i, true);
}

void Distributor::expectTerminate() {
	if (!isMaster())
		Data::expectTerminate();
}

void Distributor::terminateWorker(const int w, const bool terminate) {
	if (terminate)
		MPI_Send(nullptr, 0, MPI_INT, w, Data::TAGS::TERMINATE_TAG, MPI_COMM_CLUSTER);
	else
		MPI_Send(nullptr, 0, MPI_INT, w, Data::TAGS::RESTART_TAG, MPI_COMM_CLUSTER);
}

Distributor::Distributor(int argc, char ** argv, Node * targetNode) : ARGC(argc), ARGV(argv) {
	if (!setTargetNode(targetNode)) {
		std::cerr << std::string(COLOR_RED) + "ERROR: graph contains circles. Terminating now." + std::string(COLOR_NC) << std::endl;
		throw "ERROR: graph contains circles. Terminating now.";
	}

	if (instanceInit()) {
		std::cerr << std::string(COLOR_RED) + "ERROR: could not create instances, terminating now" + std::string(COLOR_NC) << std::endl;
		throw "ERROR: could not create instances, terminating now";
	}
}

void Distributor::receiveOutputFromWorkers() {
	if (Distributor::silent) {
		std::vector<std::string> newStdOut, newStdErr;
		int universe_size;
		MPI_Comm_remote_size(MPI_COMM_CLUSTER, &universe_size);
		newStdOut.resize(universe_size);
		newStdErr.resize(universe_size);

		std::vector<std::vector<std::string>> split; // split[0] contains instances for next execution, split[0] contains hostnames which will be terminated
		int size_after_restart = isRestarting() ? MPI_SIZE : 0;
		int size_before_restart = splitHostfile(MPI_HOSTFILE, nullptr, &split, &size_after_restart, nullptr);

		for (auto instance : split[1])
			if (instance != getName())
				output << indentLogText("initiating " + std::string((shutdownFlag)?"":"faked (just logging) ") + "shutdown for " + instance + "...");
		const unsigned SIZE = 1024 * 1024 * 4; // max value on ubuntu 17.04, g++ 5.4.0
		for (int i = 0; i < 2 * (size_before_restart - size_after_restart); ++i) {
			char* buffer = new char[SIZE];
			Data buffer_(buffer, { SIZE }, sizeof(char));
			auto status = buffer_.recv_M_from_W();

			if (status.MPI_TAG == Data::TAGS::STD_OUT_TAG) {
				if (buffer_.size()[0] >= SIZE)
					std::cerr << "WARNING: stdout from worker " + std::to_string(status.MPI_SOURCE) + " has been truncated." << std::endl;
				newStdOut[status.MPI_SOURCE] = std::string(buffer, buffer_.size()[0]);
			} else if (status.MPI_TAG == Data::TAGS::STD_ERR_TAG) {
				if (buffer_.size()[0] >= SIZE)
					std::cerr << "WARNING: stderr from worker " + std::to_string(status.MPI_SOURCE) + " has been truncated." << std::endl;
				newStdErr[status.MPI_SOURCE] = std::string(buffer, buffer_.size()[0]);
			} else {
				// the next error msg would is a bit useless -> until we deactivate silent mode again
				std::cerr << "unexpected ERROR while receiving output from workers, " + std::string(__FILE__) + ":" + std::to_string(__LINE__) << std::endl;
				exit(-1);
			}
			delete buffer;
		}

		auto tmpOutStringStream = stdOutStringStream.str();
		auto tmpErrStringStream = stdErrStringStream.str();
		stdOutStringStream.str("");
		stdErrStringStream.str("");
		for (int i = 0; i < universe_size; ++i) {
			stdOutStringStream << newStdOut[i];
			stdErrStringStream << newStdErr[i];
		}

		stdOutStringStream << tmpOutStringStream;
		stdErrStringStream << tmpErrStringStream;
	}
}

bool Distributor::saveCheckpoint(Node *n) { 
	output << n->getOutput();

	if (Distributor::silent && isMaster())
			receiveOutputFromWorkers();

	return checkpoint.save(this);
}

void Distributor::shutdownInstances(const bool shutdown, const bool silent) {
	std::vector<std::vector<std::string>> split; // split[0] contains instances for next execution, split[0] contains hostnames which should be terminated

	int size_after_restart = isRestarting() ? MPI_SIZE : 0;
	splitHostfile(MPI_HOSTFILE, nullptr, &split, &size_after_restart, nullptr);
	
	if (isRestarting() || shutdown) { // no need to leave an empty mpi.hostfile if main does not restart and instances are not shutdown
		if (split[0].size() > 0) {
			std::ofstream ofs;
			ofs.open(MPI_HOSTFILE, std::ios_base::trunc);
			for (auto instance : split[0]) {
				ofs << instance << std::endl;
			}
			ofs.close();
			if (!ofs.good()) {
				std::cerr << "ERROR: could not open " + MPI_HOSTFILE + " to write instances for resized cluster, terminating now..." << std::endl;
				exit(EXIT_FAILURE);
			}
			assert(0 == system("touch distribute")); // make distribute is triggered by changes on mpi.hostfile. But reducing does not require a re-distribute -> disabled by touching distribute.
		} else {
			auto err = std::remove(MPI_HOSTFILE.c_str());
			if (err) {
				std::cerr << "ERROR: could not delete " + MPI_HOSTFILE + " (err=" + std::to_string(err) + "), terminating now..." << std::endl;
				exit(EXIT_FAILURE);
			}
		}
	}

	std::string hosts;
	std::string hostname = getName();
	for (auto instance : split[1])
		if (instance != hostname)
			hosts += instance + " ";
	
	std::string cmd;
	if (shutdown)
		cmd = "for instance in " + hosts + "; do echo \"  initiating shutdown for $instance\" >> /dev/stdout; ssh -E /dev/stderr $instance \"sudo shutdown -P +1\"; done";
	else
		cmd = "for instance in " + hosts + "; do echo \"  initiating faked-shutdown (just logging the attempt) for $instance\" >> /dev/stdout; ssh -E /dev/stderr $instance \"echo 'now I would shutdown: '$(date) >> /tmp/distributor.shutdownSimulation\"; done";
	int err = system(cmd.c_str());
	if (err)
		std::cerr << "ERROR: could not initiate shutdown of instances (rc=" + std::to_string(err) + ")" << std::endl;
	else
		if (shutdown)
			std::cout << "...done" << std::endl;
}

std::vector<std::string> Distributor::rewriteArguments(const int argc, char ** argv, const std::string name) {
	std::vector<std::string> strings; // rewriting arguments for workers
	for (int i = 1; i < argc; ++i) // copy all arguments, but not executable name
			strings.push_back(argv[i]);

	std::string newArgString = ARGUMENT_NUM_GPUS + std::to_string(NUM_GPUS); // append number of GPUs (=slots in mpi.hostfile) as argument for workers
	strings.push_back(newArgString);
	if (Distributor::silent) { // if argument silent is used, add additional ARGUMENT_MASTER_HOSTNAME
		newArgString = ARGUMENT_MASTER_HOSTNAME + name;
		strings.push_back(newArgString);
	}

	return strings;
}

void Distributor::instanceFinalize() {
	int err = 0;
	const std::string executable(ARGV[0]);
	std::string hostName = getName();

	if (!isMaster()) {
		std::string myName = getName();
		if (Data::getLastTag() == Data::TAGS::TERMINATE_TAG) {
			std::cout << getOutput() << std::endl;
			std::cout << nowToString() + ": " + whoAmI() + ": all done" << std::endl << std::string(80, '*') << std::endl;
			if (Distributor::silent) {
				sendOutput();
				std::cout << "Output has been sent to master" << std::endl;
				deactivateSilentMode(executable, hostName);
			}
		}
		MPI_Barrier(MPI_COMM_CLUSTER);
		MPI_Finalize();
	} else {
		std::string myName = whoAmI();
		std::string name = getName();

		if (Distributor::silent && !isRestarting())
			receiveOutputFromWorkers();

		// Necessary to initiate shutdown from the master. MPI seems to kill all child processes.
		// If the child reboots, MPI (the launched child process) must not terminate before shutdown, otherwise it kills the shutdown attempt.
		// But if MPI is not killed, the shutdown will be reported as "node failure" to the MPI parent (master node).
		if (shutdownFlag)
			std::cout << ("shutting down superfluos instances now...") << std::endl;
		else
			std::cout << ("Instances will not be shut down") << std::endl;

		shutdownInstances(shutdownFlag, Distributor::silent);
		MPI_Barrier(MPI_COMM_CLUSTER);
		MPI_Finalize();

		if (isRestarting()) {
			std::string argString;
			for (auto arg : rewriteArguments(ARGC, ARGV, hostName))
				argString += arg + " ";

			// orte does not allow recursive calls :( std::string cmd = "(orte-clean; " + executable + " " + argString + " ;) &";
			std::string cmd("touch restarting"); // restart needs to be handled externally
			std::cout << "Master is starting new cluster instance now..." << std::endl;
			err = system(cmd.c_str());
			if (err)
				std::cerr << "ERROR: could not set checkfile for restart\n" << std::endl;
		} else {
			std::cout << getOutput() << std::endl;
			std::cout << nowToString() + ": " + myName + ": all done" << std::endl << std::string(80, '*') << std::endl;
			if (shutdownFlag)
				std::remove(MPI_HOSTFILE.c_str());
		}

		if (Distributor::silent)
			deactivateSilentMode(executable, "master");
	}
}

void Distributor::deactivateSilentMode(std::string executable, std::string hostname) {
#ifndef DEBUG_IGNORE_SILENT_STDOUT
	std::cout.rdbuf(stdOutBkp);
#endif
#ifndef DEBUG_IGNORE_SILENT_STDERR
		std::cerr.rdbuf(stdErrBkp);
#endif

	std::string LOGFILE;
	std::string ERROR_LOGFILE;
	if (hostname == "master") {
		LOGFILE = executable + ".out.master";
		ERROR_LOGFILE = executable + ".err.master";
	} else {
		LOGFILE = executable + ".out." + std::to_string(MPI_RANK) + "." + hostname;
		ERROR_LOGFILE = executable + ".err." + std::to_string(MPI_RANK) + "." + hostname;
	}

	std::ofstream ofsStdOut(LOGFILE, std::ios::out | std::ios::trunc);
	ofsStdOut << stdOutStringStream.str() << std::endl;
	ofsStdOut.close();
	std::ofstream ofsStdErr(ERROR_LOGFILE, std::ios::out | std::ios::trunc);
	ofsStdErr << stdErrStringStream.str() << std::endl;
	ofsStdErr.close();
}

void Distributor::estimateResizing(Node& n) {
	if (!isMaster() || resizeInProgress() || !n.isResizeable() || clusterCreation == std::chrono::steady_clock::time_point::max())
		return;
	
#ifdef DEBUG_SIMULATE_FAST_RESTART
	const int CHUNK_LIMIT = 2;
	static unsigned count = 0;
	static bool once = true;
	if (once) {
		std::cerr << "DEBUG: simulating fast restart, beginning with chunk " + std::to_string(CHUNK_LIMIT) << std::endl;
		if (clusterCreation == std::chrono::steady_clock::time_point::max())
			clusterCreation = std::chrono::steady_clock::now();
		once = false;
	}
	if (++count > CHUNK_LIMIT) {
		int new_sz = MPI_SIZE/2;
		new_sz = (new_sz == 0) ? 1 : new_sz;
		splitHostfile(MPI_HOSTFILE, nullptr, nullptr, &new_sz, nullptr);
		resizeCluster(n, new_sz);
		return;
	}
#endif // DEBUG_SIMULATE_FAST_RESTART


	const long long _elapsed_s = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - clusterCreation).count() / 1000;
	assert(_elapsed_s >= 0);
	const long long unsigned elapsed_s = static_cast<long long unsigned>(_elapsed_s);

	// estimate resize only if we come close to end of next cluster pricing time interval
	if (!(elapsed_s % CLUSTER_TIME_UNIT_SECONDS > CLUSTER_TIME_UNIT_SECONDS*CLUSTER_TIME_UNIT_MAINTENANCE_BUFFER))
		return;

	// if new_sz might become < MPI_SIZE in the next few iterations, we might initiate a restart albeit we are too close at next CLUSTER_TIME_INIT_SECONDS, ignoring defeating the buffer.
	// remembering current block_time_unit and prevent estimateResizing() until the next CLUSTER_TIME_INIT_SECONDS starts
	if (cluster_restart_block_time_unit > elapsed_s / CLUSTER_TIME_UNIT_SECONDS)
		return;

	const long long _t_elapsed_s = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - n.lastStart).count() / 1000;
	assert(_t_elapsed_s >= 0);
	const long long unsigned t_elapsed_s = static_cast<long long unsigned>(_t_elapsed_s);
	const long long unsigned t_remaining_s = static_cast<long long unsigned >(static_cast<float>(t_elapsed_s) / (n.LOOP_COUNTER - n.LOOP_COUNTER_lastStart) * (n.TOTAL_COUNT - n.LOOP_COUNTER));
	// remaining work will take more than one cluster pricing time interval - no need for resize
	if (t_remaining_s >= CLUSTER_TIME_UNIT_SECONDS)
		return;

	cluster_restart_block_time_unit = elapsed_s / CLUSTER_TIME_UNIT_SECONDS + 1; // wether if we restart or not, this cluster_time_unit block has been checked and must not be reevaluated

	int new_sz = static_cast<int>(std::ceil(static_cast<float>(MPI_SIZE*t_remaining_s / CLUSTER_TIME_UNIT_SECONDS)));
	// new_sz == 0 can happen if for debugging reasons very small CLUSTER_TIME_UNIT_SECONDS are in use
	if (new_sz == 0 || new_sz == MPI_SIZE) {
		return;
	}

	splitHostfile(MPI_HOSTFILE, nullptr, nullptr, &new_sz, nullptr);
	// new_sz is now total count of mpi processes per instance that fullfiled previous new_sz. So might be a bit higher than calculated. Checking if cluster restart is still necessary
	// also, enlarging of cluster currently not supported
	if (new_sz >= MPI_SIZE) {
		return;
	}

	resizeCluster(n, new_sz);
}

void Distributor::sendOutput() {
	if (!isMaster()) {
		std::string stdOutString = stdOutStringStream.str();
		unsigned stdOutStringLen = static_cast<unsigned int>(stdOutString.size());
		Data stdOutString_(const_cast<char*>(stdOutString.c_str()), { stdOutStringLen }, sizeof(char));
		stdOutString_.send_W_to_M(Data::TAGS::STD_OUT_TAG);

		std::string stdErrString = stdErrStringStream.str();
		unsigned stdErrStringLen = static_cast<unsigned int>(stdErrString.size());
		Data stdErrString_(const_cast<char*>(stdErrString.c_str()), { stdErrStringLen }, sizeof(char));
		stdErrString_.send_W_to_M(Data::TAGS::STD_ERR_TAG);
	}
}

std::string Distributor::deviceInfoCL() {
	char* clDevice = nullptr;
	deviceInfoOpenCL(&clDevice, assignedGPU_Device());
	std::string result("Assigned GPU device:  " + std::to_string(assignedGPU_Device()) + "\n");
	result.append(clDevice);
	free(clDevice); // clDevice comes from a c-library
	return result;
}

int Distributor::kernel_shutdown(Node &n, Distributor &distributor) {
	distributor.terminateWorkers();
	distributor.expectTerminate();
	return 0;
}