// node in work-graph
// author: Schuchardt Martin, csap9442

#include "Node.h"
#include "Distributor.h"

unsigned Node::nodeCount = 0;

const bool Node::isReady() {
	for (auto n : dependencies)
		if (!n->finished)
			return false;

	return true;
}

int Node::run(Distributor & distributor) {
	if (start == std::chrono::steady_clock::time_point::min()) // initial value, if not overwritten by checkpoint
		start = std::chrono::steady_clock::now();

	lastStart = std::chrono::steady_clock::now(); // for Distributor::estimateResizing(), calculate current workload
	LOOP_COUNTER_lastStart = LOOP_COUNTER;

	dotGraph_predecessor = (LOOP_COUNTER == 0) ? std::to_string(id) : "cluster_restart_" + std::to_string(id) + "_" + std::to_string(LOOP_COUNTER);

	int err = 0;
	if (distributor.isMaster()) {
		if (masterKernel) {
			output << logline(description, distributor.whoAmI()) << std::endl;
			err = masterKernel(*this, distributor);
		}
	} else {
		if (workerKernel) {
			output << logline(description, distributor.whoAmI()) << std::endl;
			err = workerKernel(*this, distributor);
		}
	}
	dotGraph_WorkerUnionAfterCompletion(&distributor);
	finished = true;
	end = std::chrono::steady_clock::now();

	return err;
}

std::string Node::logline(std::string line, std::string whoAmI) {
	std::string result = "  " + nowToString() + ": " + whoAmI + " computing Node(" + std::to_string(this->id) + "): " + line;
	return result;
}

const long long Node::getDuration() {
	long long elapsed_s = 0;
	if (finished)
		elapsed_s = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	else
		elapsed_s = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

	return elapsed_s;
}

std::pair<std::string, long long unsigned> Node::getDurationDetails() {
	std::string key = "Node " + std::to_string(id) + ": " + description + " (ms)";
	long long value = getDuration();
	std::pair<std::string, unsigned long long> subResult(key, value);
	return subResult;
}

void Node::dotGraph_WorkerUnionBeforeClusterRestart(Distributor* distributor) {
	std::string interimID = "cluster_restart_" + std::to_string(id) + "_" + std::to_string(LOOP_COUNTER);
	distributor->graphAppendNode(interimID, "cluster resize", true);
	for (auto w : workers_dotGraph) {
		std::string previous = "ParentNode_" + dotGraph_predecessor + "_Worker" + std::to_string(w.first) + "_" + std::to_string(w.second);
		distributor->graphAppendEdge(previous, interimID);
	}
}

void Node::dotGraph_WorkerUnionAfterCompletion(Distributor* distributor) {
	auto successors = distributor->getSuccessors(this);
	for (auto w : workers_dotGraph)
		for (auto successor : successors) {

			std::string previous = "ParentNode_" + dotGraph_predecessor + "_Worker" + std::to_string(w.first) + "_" + std::to_string(w.second);
			distributor->graphAppendEdge(previous, std::to_string(successor->getID()));
		}
}

void Node::dotGraph_AppendWorkerChunk(const int worker, Distributor* distributor) {
	if (workers_dotGraph.find(worker) == workers_dotGraph.end()) {
		std::string newNode = "ParentNode_" + dotGraph_predecessor + "_Worker" + std::to_string(worker) + "_0";
		distributor->graphAppendNode(newNode, "W" + std::to_string(worker), false);
		distributor->graphAppendEdge(dotGraph_predecessor, newNode);
		workers_dotGraph[worker] = 0;
	} else {
		int last = workers_dotGraph[worker];
		std::string previous = "ParentNode_" + dotGraph_predecessor + "_Worker" + std::to_string(worker) + "_" + std::to_string(last);
		std::string next = "ParentNode_" + dotGraph_predecessor + "_Worker" + std::to_string(worker) + "_" + std::to_string(last + 1);
		distributor->graphAppendNode(next, "W" + std::to_string(worker), false);
		distributor->graphAppendEdge(previous, next);
		workers_dotGraph[worker] = ++last;
	}
}



