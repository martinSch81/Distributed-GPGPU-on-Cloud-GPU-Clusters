// Matrix multiplication (using libraries' extra gadgets)
// author: Schuchardt Martin, csap9442

#include "Distributor.h"
#include "mmul.h"
#include "../utils/Utils.h"
#include "../utils/cl_utils.h"

#include <iostream>
#include <algorithm>


#ifndef MAX_ROWS_PER_WORKER
#define MAX_ROWS_PER_WORKER 128u
#endif

using namespace std;


// in: arrays A and B (reserved space only)
// filling A with random values and B is unit-Matrix
// out: rewritten A and B matrices
// master only
int kernel_InitMatrix(Node &n, Distributor &d) {
	DATA_TYPE *A = static_cast<DATA_TYPE*>(d.getArgument("A")->get());
	DATA_TYPE *B = static_cast<DATA_TYPE*>(d.getArgument("B")->get());
	const unsigned N = *static_cast<unsigned*>(d.getArgument("N")->get());
	
	initMatrices(A, B, N);

	return 0;
}

// in: matrix sizes N, pointer to B-matrix
// distribute B-matrix to all cluster nodes
// out: nothing
// same kernel for master and workers
int kernel_DistributeB(Node &n, Distributor &d) {
	auto err = d.getArgument("B")->bcast_M_to_W();
	return err;
}

// in: B-matrix, matrix size N from B-matrix
// waits for chunks from master, calculate subresult for C-matrix for that chunk, sends back chunk and waits for next chunk or terminate tag.
// out: chunks of C-matrix
// worker kernel
int kernel_ComputeOnWorkers(Node &n, Distributor &d) {
	const unsigned N = *static_cast<unsigned*>(d.getArgument("N")->get());
	int err = 0;
	DATA_TYPE *cBuffer = new DATA_TYPE[MAX_ROWS_PER_WORKER*N];
	while (true) {
		Data aChunk(new DATA_TYPE[MAX_ROWS_PER_WORKER*N], { MAX_ROWS_PER_WORKER, N }, sizeof(DATA_TYPE));
		MPI_Status status = aChunk.recv_W_from_M();

		if (status.MPI_TAG == Data::TAGS::TERMINATE_TAG) { // all kernels have been computed by some workers. No further work for this kernel.
			n.addOutput(indentLogText("  no more work for this worker on this kernel anymore"));
			break;
		} else if (status.MPI_TAG == Data::TAGS::RESTART_TAG) {
			n.addOutput(indentLogText("  cluster will restart.\n  Saving checkpoint and stopping now but expecting to continue."));
			if (!d.saveCheckpoint(&n)) {
				cerr << "ERROR: could not write checkpoint (" + CHECKPOINT_FILE + "). Terminating now." << endl;
				exit(EXIT_FAILURE);
			}
			break;
		}

		// multiplyChunkCPU(static_cast<DATA_TYPE*>(aChunk.get()), aChunk.sizeTotal() / N, N, static_cast<DATA_TYPE*>(distributor.getArgument("B")->get()), cBuffer); // legacy, simple mmul using CPU
		multiplyChunkCL(static_cast<DATA_TYPE*>(aChunk.get()), aChunk.sizeTotal() / N, N, static_cast<DATA_TYPE*>(d.getArgument("B")->get()), cBuffer, d.assignedGPU_Device());
		Data cChunk(cBuffer, aChunk.size(), sizeof(DATA_TYPE));
		err |= cChunk.send_W_to_M(Data::TAGS::RECEIVE_CHUNK_TAG);

		n.addOutput(indentLogText("Worker " + to_string(d.getRank()) + " executes " + n.getDescription()));
	}

	delete[] cBuffer;
	return err;
}

// if no idle workers available or all chunks have been distributed:
// wait for incoming chunk (from any worker), get matching chunk-id associated to the worker-id, fill C-matrix with chunk at the matching position
void waitForWorker(Node& n, Distributor &d, map<int, int> &worker_chunks, vector<Data*> &cChunks) {
	const unsigned N = *static_cast<unsigned*>(d.getArgument("N")->get());
	Data cBuffer(new DATA_TYPE[MAX_ROWS_PER_WORKER*N], { MAX_ROWS_PER_WORKER, N }, sizeof(DATA_TYPE));
	auto status = cBuffer.recv_M_from_W(MPI_ANY_SOURCE, Data::TAGS::RECEIVE_CHUNK_TAG);


	DATA_TYPE *cTarget = static_cast<DATA_TYPE*>(cChunks[worker_chunks.at(status.MPI_SOURCE)]->get());
	DATA_TYPE *cChunk = static_cast<DATA_TYPE*>(cBuffer.get());
	for (unsigned i = 0; i < cBuffer.sizeTotal(); ++i) // need to copy the data, because we don't know the sender and so the target pointer until we've received the data
		cTarget[i] = cChunk[i];

	delete[] static_cast<DATA_TYPE*>(cBuffer.get());
	d.addToIdleQueue(n, status.MPI_SOURCE); // mark worker as idle again
}


// in: A, B and C-matrix, N from C-matrix
// split A (and C) into chunks, distribute A-chunk to workers, wait for computed C-chunks
// out: computed C-matrix
// master kernel. inserts additionally nodes in .dot-graph for each chunk/worker
int kernel_ComputeOnMaster(Node &n, Distributor &d) {
	const unsigned N = *static_cast<unsigned*>(d.getArgument("N")->get());
	unsigned ROWS = std::min(N / d.getSize(), static_cast<unsigned>(MAX_ROWS_PER_WORKER)); // split, at least one part per worker, but chunk has at most MAX_ROWS_PER_WORKER lines
	ROWS = std::max(ROWS, 1u); // at least one row per node if N is less then worker-nodes

	auto aChunks = d.getArgument("A")->sliceSize(ROWS*N);
	auto cChunks = d.getArgument("C")->sliceSize(ROWS*N);
	n.addOutput(indentLogText("distributing " + to_string(aChunks.size()) + " chunks to workers..."));

	assert(aChunks.size() <= std::numeric_limits<unsigned int>::max());
	n.activateAutoResize(static_cast<unsigned>(aChunks.size())); // tell the library how many chunks exist, then need to use *getLoopCounter() to increment the current status
	unsigned *loopCounter = n.getLoopCounter();

	int err = 0;
	map<int, int> worker_chunks; // to remember which worker computed which C-chunk
	
	for (; *loopCounter < aChunks.size(); ++*loopCounter) {
		auto worker = d.nextWorker(&n);
		while (worker == Distributor::NO_IDLE_WORKERS_AVAILABLE) { // no idle worker currently available
			if (d.isRestarting()) // if a restart has been initiated, we will never get an idle worker and have to terminate
				return err;
			waitForWorker(n, d, worker_chunks, cChunks); // otherwise, we wait for the next worker to finish & insert computed chunk in C-matrix
			worker = d.nextWorker(&n);
		}

		err |= aChunks[*loopCounter]->send_M_to_W(worker, Data::TAGS::SEND_CHUNK_TAG);
		worker_chunks[worker] = *loopCounter;
		n.addOutput(indentLogText("distributed chunk " + to_string(*loopCounter + 1) + " to worker " + to_string(worker)));
	}

	while (d.availableWorkers() != (d.getSize()))
		waitForWorker(n, d, worker_chunks, cChunks);

	d.terminateWorkers(); // nothing to do for workers anymore

	return err;
}


// in: A and C-matrix, N from A-matrix
// compare A == C, cout errors
// out: 1 if A and C do not match, else 0
int kernel_Verify(Node &n, Distributor &d) {
	const unsigned N = *static_cast<unsigned*>(d.getArgument("N")->get());
	DATA_TYPE *A = static_cast<DATA_TYPE*>(d.getArgument("A")->get());
	DATA_TYPE *C = static_cast<DATA_TYPE*>(d.getArgument("C")->get());
	
	string output;
	int err = testResults(A, C, N, output);
	if (err) {
		cerr << string(COLOR_RED) + "ERROR: verify failed, results do not match" + string(COLOR_NC) << endl;
		output += string(COLOR_RED) + "ERROR: verify failed, results do not match" + string(COLOR_NC) + '\n';
	} else
		output += string(COLOR_GREEN) + "results verified, results are correct" + string(COLOR_NC) + '\n';
	n.addOutput(indentLogText(output));

	return err;
}


int main(int argc, char** argv) {
	unsigned N = 727; // small size for fast testing
	string arg;
	if (parseArguments(argc, argv, ARGUMENT_N, arg) >= 0)
		N = atoi(arg.c_str());
	
	string mpiVersion;
	if (!getMPI_StandardVersion(1, 6, mpiVersion)) {
		cerr << string(COLOR_RED) + "ERROR: incompatible MPI version " + mpiVersion + " found, expecting at least v1.6. Terminating now." + string(COLOR_NC) << endl;
		exit(EXIT_FAILURE);
	}

	Node *initMatrix = new Node("init input matrices", kernel_InitMatrix, nullptr);
	Node *distributeB = new Node("distribute matrix B", kernel_DistributeB, kernel_DistributeB);
	Node *compute = new Node("computing chunks", kernel_ComputeOnMaster, kernel_ComputeOnWorkers);
	Node *verify = new Node("verify", kernel_Verify, nullptr);

	distributeB->addDependency(initMatrix);
	compute->addDependency(distributeB);
	verify->addDependency(compute);

	// verify does not need workers any more.
	// Better setRoot(compute), then call distributor.instanceFinalize; to shutdown workers and at last compute verify with master as last instance running.
	// But verify as root makes this toy example better to test functionality.
	Distributor d(argc, argv, verify);
	// d.setShutdownFlag(true); // shutdown flag: true=shutdown, false(default)=log shutdown attempt only

	Data *A_ = nullptr, *C_ = nullptr;
	DATA_TYPE *A = nullptr, *C = nullptr;
	if (d.isMaster()) {
		try {
			A = new DATA_TYPE[N*N];
			C = new DATA_TYPE[N*N];
		} catch (const std::bad_alloc& e) {
			cerr << string(COLOR_RED) + d.whoAmI() + ": ERROR: could not allocate space for N*N=" + to_string(N) + "*" + to_string(N) + " elements. Error message: " + string(e.what()) + ". Terminating now." + string(COLOR_NC) << endl;
			return -2;
		}
		A_ = new Data(A, { N, N }, sizeof(DATA_TYPE));
		C_ = new Data(C, { N, N }, sizeof(DATA_TYPE));
		d.addArguments({ { "A", A_ },{ "C", C_ } });
	}
	DATA_TYPE *B;
	try {
		B = new DATA_TYPE[N*N];
	} catch (const std::bad_alloc& e) {
		cerr << string(COLOR_RED) + d.whoAmI() + ": ERROR: could not allocate space for N*N=" + to_string(N) + "*" + to_string(N) + " elements. Error message: " + string(e.what()) + ". Terminating now." + string(COLOR_NC) << endl;
		return -2;
	}
	Data B_(B, { N, N }, sizeof(DATA_TYPE));
	Data N_(&N, { 1 }, sizeof(unsigned));
	d.addArguments({ { "B", &B_ }, { "N", &N_}});
	
	if (d.isMaster()) {
		string mpiVersion;
		getMPI_StandardVersion(0, 0, mpiVersion);
		cout << string(COLOR_YELLOW) + "  MPI(v" << mpiVersion << ") cluster size: " << d.getSize() << endl;
		cout << "  Matrix multiplication, using " << N << "x" << N << " matrix (" << DATA_TYPE_STRING << "), max chunk size " << to_string(MAX_ROWS_PER_WORKER*N) << string(COLOR_NC) << endl << endl;
	} else {
		d.addOutput(indentLogText(d.deviceInfoCL()));
	}

	if (d.getSize() < 1) {
		cerr << string(COLOR_RED) + "ERROR: distributed matrix multiplication needs at least 2 nodes; one master and [1...N] workers" + string(COLOR_NC) << endl;
		exit(EXIT_FAILURE);
	}
	
	int err = d.run();

	if (d.isMaster() && !d.isRestarting()) {
		cout << string(COLOR_GREEN) + "  elapsed time for MPI matrix multiplication: \t" << to_string(d.getDuration()) << "  ms" + string(COLOR_NC) << endl;
		if (err)
			cerr << string(COLOR_RED) + "ERROR: finished with RC=" + to_string(err) + string(COLOR_NC) << endl << std::string(80, '*') << endl << endl << endl;
		else
			cout << string(COLOR_GREEN) + "finished with RC=" + to_string(err) + string(COLOR_NC) << endl << std::string(80, '*') << endl;
	}
	if (!d.isRestarting()) {
		// cluster_size will become less meaningful if restarts occure. Still useful for benchmarking.
		writeCSV(genCSVFileName(argv[0], d.getRank()), { pair<string, unsigned>("num_gpus", Distributor::getNumGPUs()), pair<string, unsigned>("cluster_size", d.getSize()), pair<string, unsigned>("N", N)}, d.getDurationDetails());
	}

	delete initMatrix; delete distributeB; delete compute; delete verify;
	delete[] static_cast<DATA_TYPE*>(B_.get());
	if (d.isMaster()) {
		delete[] static_cast<DATA_TYPE*>(A_->get());
		delete[] static_cast<DATA_TYPE*>(C_->get());
	}
	exit (err);
}
