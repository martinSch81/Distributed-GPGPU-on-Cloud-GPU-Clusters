// data object to be distributed
// author: Schuchardt Martin, csap9442

#include "Data.h"

long long unsigned Data::duration_bcast_M_to_W = 0;
long long unsigned Data::duration_bcast_W_to_W = 0;
long long unsigned Data::duration_send_M_to_W = 0;
long long unsigned Data::duration_send_W_to_M = 0;
long long unsigned Data::duration_send_W_to_W = 0;
long long unsigned Data::duration_recv_W_from_M = 0;
long long unsigned Data::duration_recv_M_from_W = 0;
long long unsigned Data::duration_recv_W_from_W = 0;
long long unsigned Data::duration_gather_W_to_M = 0;
long long unsigned Data::duration_allGather_W_to_W = 0;
long long unsigned Data::duration_reduce_W_to_M = 0;
long long unsigned Data::duration_allReduce_W_to_W = 0;

Data::TAGS Data::tag = Data::TAGS::UNDEFINED_TAG;


const unsigned Data::sizeTotal() {
	unsigned result = 1;
	for (auto &s : sz)
		result *= s;
	return result;
}

std::vector<Data*> Data::sliceSize(unsigned size) {
	std::vector<Data*> result;

	unsigned count = 0;
	const auto sizeTotal = this->sizeTotal();
	while (count < sizeTotal) {
		char* chunkPtr = static_cast<char*>(data) + (count * this->sizeOf);
		unsigned chunkSize = (count + size) <= sizeTotal ? size : (sizeTotal - count);
		result.push_back(new Data(chunkPtr, { chunkSize }, sizeOf));
		count += size;
	}

	return result;
}

std::vector<Data*> Data::sliceParts(unsigned parts) {
	std::vector<Data*> result;

	unsigned count = 0;
	const auto sizeTotal = this->sizeTotal();
	const unsigned partSize = (sizeTotal % parts == 0) ? sizeTotal / parts : (sizeTotal / parts) + 1;
	while (count < sizeTotal) {
		char* chunkPtr = static_cast<char*>(data) + (count * this->sizeOf);
		unsigned chunkSize = (count + partSize) <= sizeTotal ? partSize : (sizeTotal - count);
		result.push_back(new Data(chunkPtr, { chunkSize }, sizeOf));
		count += partSize;
	}

	return result;
}

int Data::bcast_M_to_W() {
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
	auto err = bcast(DISTRIBUTOR_ROOT_NODE, MPI_COMM_CLUSTER);
	duration_bcast_M_to_W += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
	return err;
}

int Data::bcast_W_to_W(const int source) { 
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
	auto err = bcast(source, MPI_COMM_WORLD);
	duration_bcast_W_to_W += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
	return err;
}
int Data::send_M_to_W(const int receiver, const int tag) { 
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
	auto err = send(receiver, tag, MPI_COMM_CLUSTER);
	duration_send_M_to_W += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
	return err;
}
int Data::send_W_to_M(const int tag) { 
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
	auto err = send(DISTRIBUTOR_ROOT_NODE, tag, MPI_COMM_CLUSTER);
	duration_send_W_to_M += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
	return err;
}
int Data::send_W_to_W(const int receiver, const int tag) { 
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
	auto err = send(receiver, tag, MPI_COMM_WORLD);
	duration_send_W_to_W += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
	return err;
}
MPI_Status Data::recv_W_from_M(const int tag) { 
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
	auto result = recv(DISTRIBUTOR_ROOT_NODE, tag, MPI_COMM_CLUSTER);
	duration_recv_W_from_M += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
	return result;
}
MPI_Status Data::recv_M_from_W(const int source, const int tag) { 
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
	auto result = recv(source, tag, MPI_COMM_CLUSTER);
	duration_recv_M_from_W += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
	return result;
}
MPI_Status Data::recv_W_from_W(const int source, const int tag) {
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
	auto result = recv(source, tag, MPI_COMM_WORLD);
	duration_recv_W_from_W += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
	return result;
}

int Data::gather_W_to_M(void* result) {
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

	int nbrItems = sizeTotal();
	if (MPI_COMM_WORKER_TO_WORKER == MPI_COMM_NULL)
		nbrItems = sizeTotal() / DISTRIBUTOR_MPI_SIZE;

	auto err = gather(result, nbrItems, DISTRIBUTOR_ROOT_NODE, MPI_COMM_CLUSTER);
	duration_gather_W_to_M += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
	return err;
}

int Data::allGather_W_to_W(void* result) {
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

	int nbrItems = sizeTotal() / DISTRIBUTOR_MPI_SIZE;
	auto err = allGather(result, nbrItems, MPI_COMM_WORKER_TO_WORKER);

	duration_allGather_W_to_W += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
	return err;
}

int Data::reduce_W_to_M(MPI_Datatype datatype, MPI_Op op) {
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

	auto err = reduce(sizeTotal(), DISTRIBUTOR_ROOT_NODE, datatype, op, MPI_COMM_CLUSTER);
	
	duration_reduce_W_to_M += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
	return err;
}

int Data::allReduce_W_to_W(void* result, MPI_Datatype datatype, MPI_Op op) {
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

	auto err = allReduce(result, datatype, op, MPI_COMM_WORKER_TO_WORKER);
	
	duration_allReduce_W_to_W += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
	return err;
}


std::vector<std::pair<std::string, long long unsigned>> Data::getDurationDetails() {
	std::vector<std::pair<std::string, unsigned long long>> result;

	result.push_back(std::pair<std::string, unsigned long long>("bcast M_to_W (ms)    ", duration_bcast_M_to_W));
	result.push_back(std::pair<std::string, unsigned long long>("bcast W_to_W (ms)    ", duration_bcast_W_to_W));
	result.push_back(std::pair<std::string, unsigned long long>("send M_to_W (ms)     ", duration_send_M_to_W));
	result.push_back(std::pair<std::string, unsigned long long>("send W_to_M (ms)     ", duration_send_W_to_M));
	result.push_back(std::pair<std::string, unsigned long long>("send W_to_W (ms)     ", duration_send_W_to_W));
	result.push_back(std::pair<std::string, unsigned long long>("recv W_from_M (ms)   ", duration_recv_W_from_M));
	result.push_back(std::pair<std::string, unsigned long long>("recv M_from_W (ms)   ", duration_recv_M_from_W));
	result.push_back(std::pair<std::string, unsigned long long>("recv W_from_W (ms)   ", duration_recv_W_from_W));
	result.push_back(std::pair<std::string, unsigned long long>("gather W_to_M (ms)   ", duration_gather_W_to_M));
	result.push_back(std::pair<std::string, unsigned long long>("allGather W_to_W (ms)", duration_allGather_W_to_W));
	result.push_back(std::pair<std::string, unsigned long long>("reduce W_to_M (ms)   ", duration_reduce_W_to_M));
	result.push_back(std::pair<std::string, unsigned long long>("allReduce W_to_W (ms)", duration_allReduce_W_to_W));

	return result;
}

int Data::bcast(const int source, const MPI_Comm comm) {
	auto err = MPI_Bcast(data, sizeOf*sizeTotal(), MPI_BYTE, source, comm);
	return err;
}

int Data::send(const int receiver, const int tag, const MPI_Comm comm) {
	auto err = MPI_Send(data, sizeOf*sizeTotal(), MPI_BYTE, receiver, tag, comm);
	return err;
}

MPI_Status Data::recv(const int source, const int tag, const MPI_Comm comm) {
	MPI_Status status;
	MPI_Recv(data, sizeOf*sizeTotal(), MPI_BYTE, source, tag, comm, &status);

	int iReceived;
	MPI_Get_count(&status, MPI_BYTE, &iReceived);
	iReceived /= sizeOf;

	sz.clear();
	sz.push_back(iReceived);

	setLastTag(status.MPI_TAG);

	return status;
}

int Data::gather(void* result, const int nbrItems, const int receiver, const MPI_Comm comm) {
	auto err = MPI_Gather(data, nbrItems*sizeOf, MPI_BYTE, result, nbrItems*sizeOf, MPI_BYTE, receiver, comm);
	return err;
}

int Data::allGather(void* result, const int nbrItems, const MPI_Comm comm) {
	auto err = MPI_Allgather(data, nbrItems*sizeOf, MPI_BYTE, result, nbrItems*sizeOf, MPI_BYTE, comm);
	return err;
}

int Data::reduce(const int nbrItems, const int receiver, const MPI_Datatype datatype, const MPI_Op op, MPI_Comm comm) {
	auto err = 0;
	if (MPI_COMM_WORKER_TO_WORKER == MPI_COMM_NULL)
		err = MPI_Reduce(nullptr, data, nbrItems, datatype, op, receiver, comm);
	else
		err = MPI_Reduce(data, nullptr, nbrItems, datatype, op, receiver, comm);

	return err;
}

int Data::allReduce(void* result, const MPI_Datatype datatype, const MPI_Op op, MPI_Comm comm) {
	auto err = MPI_Allreduce(data, result, sizeTotal(), datatype, op, comm);
	return err;
}

Data::TAGS Data::expectTerminate() {

	int foo = 815;
	Data dummy(&foo, { 1 }, sizeof(int));
	auto status = dummy.recv_W_from_M();
	return static_cast<Data::TAGS>(status.MPI_TAG);
}
