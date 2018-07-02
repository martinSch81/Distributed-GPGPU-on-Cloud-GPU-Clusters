// simple Matrix multiplication
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


// filling A with random values and B is unit-Matrix (master only)
int kernel_InitMatrix(Node &n, Distributor &d) {
	int *A = static_cast<int*>(d.getArgument("A")->get());
	int *B = static_cast<int*>(d.getArgument("B")->get());
	const unsigned N = *static_cast<unsigned*>(d.getArgument("N")->get());
	
	initMatrices(A, B, N);

	return 0;
}

// distribute B-matrix to all cluster nodes (same code for master and workers)
int kernel_DistributeB(Node &n, Distributor &d) {
	auto err = d.getArgument("B")->bcast_M_to_W();
	return err;
}

// main worker kernel
// waiting for chunks from master, calculate subresult for C-matrix for that chunk, sends back chunk and waits for next chunk or terminate tag.
int kernel_ComputeOnWorkers(Node &n, Distributor &d) {
	const unsigned N = *static_cast<unsigned*>(d.getArgument("N")->get());
	int err = 0;
	int *cBuffer = new int[MAX_ROWS_PER_WORKER*N]; // to store the result for our chunk
	while (true) { // wait for chunks until we receive a terminate flag from the master
		Data aChunk(new int[MAX_ROWS_PER_WORKER*N], { MAX_ROWS_PER_WORKER, N }, sizeof(int));
		MPI_Status status = aChunk.recv_W_from_M();

		if (status.MPI_TAG == Data::TAGS::TERMINATE_TAG) { // all kernels have been computed by some workers. No further work for this kernel.
			n.addOutput(indentLogText("  no more work for this worker on this kernel anymore"));
			break;
		}

		multiplyChunkCL(static_cast<int*>(aChunk.get()), aChunk.sizeTotal() / N, N, static_cast<int*>(d.getArgument("B")->get()), cBuffer, d.assignedGPU_Device());
		Data cChunk(cBuffer, aChunk.size(), sizeof(int)); // wrapping the computed c-Chunk into a Data object for easy transmission to Master
		err |= cChunk.send_W_to_M(Data::TAGS::RECEIVE_CHUNK_TAG);

		n.addOutput(indentLogText("Worker " + to_string(d.getRank()) + " executes " + n.getDescription()));
	}

	delete[] cBuffer;
	return err;
}

// if no idle workers are available or all chunks have been distributed:
// wait for incoming chunk (from any worker), get matching chunk-id associated to the worker-id, fill C-matrix with chunk at the matching position
void waitForWorker(Node& n, Distributor &d, map<int, int> &worker_chunks, vector<Data*> &cChunks) {
	const unsigned N = *static_cast<unsigned*>(d.getArgument("N")->get());

	// A buffer for incoming data. We don't know before receiving which chunk from which worker we will receive.
	Data cBuffer(new int[MAX_ROWS_PER_WORKER*N], { MAX_ROWS_PER_WORKER, N }, sizeof(int));
	auto status = cBuffer.recv_M_from_W(MPI_ANY_SOURCE, Data::TAGS::RECEIVE_CHUNK_TAG);

	unsigned currentWorkersChunk = worker_chunks.at(status.MPI_SOURCE); // worker_chunks maps worker-ids with computed c-chunk-id
	int *cTarget = static_cast<int*>(cChunks[currentWorkersChunk]->get());
	int *cChunk = static_cast<int*>(cBuffer.get());
	for (unsigned i = 0; i < cBuffer.sizeTotal(); ++i) // now we need to copy the data, from the buffer to the pointer at the correct position of our final C-matrix
		cTarget[i] = cChunk[i];

	delete[] static_cast<int*>(cBuffer.get());
	d.addToIdleQueue(n, status.MPI_SOURCE); // We have received and stored the data, the now idle worker may be reused again.
}


// main master kernel
// split A (and C) into chunks, distribute A-chunk to workers, wait for computed C-chunks
int kernel_ComputeOnMaster(Node &n, Distributor &d) {
	const unsigned N = *static_cast<unsigned*>(d.getArgument("N")->get());
	unsigned ROWS_PER_CHUNK = std::min(N / d.getSize() / 4, static_cast<unsigned>(MAX_ROWS_PER_WORKER)); // we want to split the total work in 4 parts per worker, but not exceed an upper limit (MAX_ROWS_PER_WORKER)
	
	auto aChunks = d.getArgument("A")->sliceSize(ROWS_PER_CHUNK*N); // split the source data into chunks with size ROWS_PER_CHUNK*N (last part may be smaller)
	auto cChunks = d.getArgument("C")->sliceSize(ROWS_PER_CHUNK*N);
	n.addOutput(indentLogText("distributing " + to_string(aChunks.size()) +" chunks to workers..."));

	int err = 0;
	map<int, int> worker_chunks; // to remember which worker computed which C-chunk
	
	// distribute all chunks. We have more chunks than workers, so once all workers are busy, we need to wait for the first to finish its computation
	for (unsigned i = 0; i < aChunks.size(); ++i) {
		auto worker = d.nextWorker(&n);
		while (worker == Distributor::NO_IDLE_WORKERS_AVAILABLE) { // if no idle worker currently available, ...
			waitForWorker(n, d, worker_chunks, cChunks); // ...we wait for the next worker to finish & insert its computed chunk in the resulting C-matrix
			worker = d.nextWorker(&n); // Now a worker is idle and ready to receive another chunk again.
		}

		err |= aChunks[i]->send_M_to_W(worker, Data::TAGS::SEND_CHUNK_TAG); // master to worker transmission of chunk
		worker_chunks[worker] = i; // remember which worker computed which chunk to know where to insert the computed cChunk later
		n.addOutput(indentLogText("distributed chunk " + to_string(i + 1) + " to worker " + to_string(worker)));
	}

	while (d.availableWorkers() != (d.getSize())) // wait for the currently busy workers for their computed chunks
		waitForWorker(n, d, worker_chunks, cChunks);

	d.terminateWorkers(); // nothing to do for workers anymore

	return err;
}


// B is identity matrix, so A should equal C
int kernel_Verify(Node &n, Distributor &d) {
	const unsigned N = *static_cast<unsigned*>(d.getArgument("N")->get());
	int *A = static_cast<int*>(d.getArgument("A")->get());
	int *C = static_cast<int*>(d.getArgument("C")->get());
	
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
	Node *initMatrix = new Node("init input matrices", kernel_InitMatrix, nullptr); // define the nodes (=computation steps)
	Node *distributeB = new Node("distribute matrix B", kernel_DistributeB, kernel_DistributeB);
	Node *compute = new Node("computing chunks", kernel_ComputeOnMaster, kernel_ComputeOnWorkers);
	Node *verify = new Node("verify", kernel_Verify, nullptr);

	distributeB->addDependency(initMatrix); // add dependencies for all nodes
	compute->addDependency(distributeB);
	verify->addDependency(compute);

	Distributor d(argc, argv, verify); // initiate/launch cluster, set 'verify' as that shall be computed (after all of its dependencies)

	unsigned N = 727; // small size for fast testing
	Data *A_ = new Data(new int[N*N], { N, N }, sizeof(int)); // our three data matrices: A*B=C
	Data *B_ = new Data(new int[N*N], { N, N }, sizeof(int));
	Data *C_ = new Data(new int[N*N], { N, N }, sizeof(int));
	Data N_(&N, { 1 }, sizeof(unsigned));
	d.addArguments({ { "A", A_ },{ "C", C_ } ,{ "B", B_ }, { "N", &N_}}); // adding all data objects to the library, to be accessible in the nodes
	
	int err = d.run(); // start computation, terminate cluster at the end
	
	delete initMatrix; delete distributeB; delete compute; delete verify; // cleanup
	delete[] static_cast<int*>(A_->get());
	delete[] static_cast<int*>(C_->get());
	delete[] static_cast<int*>(B_->get());

	exit (err);
}
