// quick tutorial how to migrate single-server code to a distributed version
// author: Schuchardt Martin, csap9442

#include <iostream>
#include <string>

#include "../distributor/Distributor.h"
#include "../distributor/Checkpoint.h"

using namespace std;

const unsigned N = 27;
const unsigned PARTS = 5;
const int TAG = 815;

//void init(int *input) {
int kernel_init(Node &n, Distributor &d) {
	int *input = static_cast<int*>(d.getArgument("*input")->get());

	for (unsigned i = 0; i < N; ++i)
		input[i] = i;

	return 0;
}

int receiveData(Node &n, Distributor &d) {
	int subresult = 0;
	Data subresult_(&subresult, { 1 }, sizeof(subresult));

	auto status = subresult_.recv_M_from_W(MPI_ANY_SOURCE);
	d.addToIdleQueue(n, status.MPI_SOURCE); // mark worker as idle again
	return subresult;
}

//void calc(int *input, int *result) {
int kernel_calc_master(Node &n, Distributor &d) {
	int *result = static_cast<int*>(d.getArgument("*result")->get());
	*result = 0;

	Data *input_ = d.getArgument("*input");
	auto inputChunks = input_->sliceParts(PARTS);

	for (auto chunk : inputChunks) {
		auto worker = d.nextWorker(&n);
		while (worker == Distributor::NO_IDLE_WORKERS_AVAILABLE) { // no idle worker currently available
			*result += receiveData(n, d);
			worker = d.nextWorker(&n);
		}

		chunk->send_M_to_W(worker, TAG);
		n.addOutput(indentLogText("distributed chunk to worker " + to_string(worker)));
	}

	while (d.availableWorkers() != (d.getSize()))
		*result += receiveData(n, d);

	return 0;
}

//void calc(int *input, int *result) {
int kernel_calc_worker(Node &n, Distributor &d) {
	int result;
	Data result_(&result, { 1 }, sizeof(result));
	Data chunk(new int[N], { N }, sizeof(result));

	while (true) {
		auto status = chunk.recv_W_from_M();
		if (status.MPI_TAG == Data::TAGS::TERMINATE_TAG) { // all kernels have been computed by some workers. No further work for this kernel.
			n.addOutput(indentLogText("  no more work for this worker on this kernel anymore"));
			break;
		}
		int* input = static_cast<int*>(chunk.get());
		n.addOutput(indentLogText("computing one chunk..."));
		result = 0;
		for (unsigned i = 0; i < chunk.sizeTotal(); ++i)
			result += input[i];
		
		result_.send_W_to_M(TAG);
	}

	delete static_cast<int*>(chunk.get());
	return 0;
}

// void verify(int *input, int *result) {
int kernel_verify(Node &n, Distributor &d) {
	int *input = static_cast<int*>(d.getArgument("*input")->get());
	int *result = static_cast<int*>(d.getArgument("*result")->get());

	int should = 0;
	for (unsigned i = 0; i < N; ++i)
		should += input[i];

	if (*result == should) {
		n.addOutput(indentLogText("OK: calculated " + to_string(*result) + ", expected " + to_string(should)));
		return 0;
	}

	n.addOutput(indentLogText("ERROR: calculated " + to_string(*result) + ", expected " + to_string(should)));
	return 1;
}


int main(int argc, char** argv) {
	int input[N];
	int result;

	Node *init = new Node("init", kernel_init, nullptr);
	Node *calculate = new Node("calculate", kernel_calc_master, kernel_calc_worker);
	Node *shutdown = new Node("shutdown instances", Distributor::kernel_shutdown, nullptr);
	Node *verify = new Node("verify", kernel_verify, nullptr);
	calculate->addDependency(init);
	shutdown->addDependency(calculate);
	verify->addDependency(shutdown);

	Distributor d(argc, argv, verify);

	Data input_(&input, { N }, sizeof(int));
	Data result_(&result, { 1 }, sizeof(result));
	d.addArguments({ { "*input", &input_ },{ "*result", &result_ } });

	int err = d.run();

	// init(input);
	// calc(input, &result);
	// verify(input, &result);

	delete init; delete calculate; delete shutdown; delete verify;

	exit (err);
}