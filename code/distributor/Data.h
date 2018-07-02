// data object to be distributed
// author: Schuchardt Martin, csap9442

#pragma once

#include <vector>
#include <utility>
#include <chrono>
#include <mpi.h>

extern MPI_Comm MPI_COMM_CLUSTER;
extern MPI_Comm MPI_COMM_WORKER_TO_WORKER;
extern int DISTRIBUTOR_ROOT_NODE;
extern int DISTRIBUTOR_MPI_SIZE;

class Data {
	friend class Checkpoint;

private:
public:
	enum TAGS {
		UNDEFINED_TAG = -1, SEND_CHUNK_TAG = 0, RECEIVE_CHUNK_TAG = 1, RESTART_TAG = 2, TERMINATE_TAG = 3, STD_OUT_TAG = 4, STD_ERR_TAG = 5
	};

private:
	static TAGS tag;

	void* data;
	std::vector<unsigned> sz;
	const unsigned sizeOf;

	static long long unsigned duration_bcast_M_to_W;
	static long long unsigned duration_bcast_W_to_W;
	static long long unsigned duration_send_M_to_W;
	static long long unsigned duration_send_W_to_M;
	static long long unsigned duration_send_W_to_W;
	static long long unsigned duration_recv_W_from_M;
	static long long unsigned duration_recv_M_from_W;
	static long long unsigned duration_recv_W_from_W;
	static long long unsigned duration_gather_W_to_M;
	static long long unsigned duration_allGather_W_to_W;
	static long long unsigned duration_reduce_W_to_M;
	static long long unsigned duration_allReduce_W_to_W;

	int bcast(const int source, const MPI_Comm comm);
	int send(const int receiver, const int tag, const MPI_Comm comm);
	MPI_Status recv(const int source, const int tag, const MPI_Comm comm);
	int gather(void* result, const int nbrItems, const int receiver, const MPI_Comm comm);
	int allGather(void *result, const int nbrItems, const MPI_Comm comm);
	int reduce(const int nbrItems, const int receiver, const MPI_Datatype datatype, const MPI_Op op, const MPI_Comm comm);
	int allReduce(void* result, const MPI_Datatype datatype, const MPI_Op op, const MPI_Comm comm);

public:
	Data(void* _data, std::vector<unsigned> _size, unsigned _sizeOf) : data(_data), sz(_size), sizeOf(_sizeOf) {};

	void* get() { return data; }
	Data* set(void* _data) { data = _data; return this; }
	const std::vector<unsigned>& size() { return sz; };
	const unsigned sizeTotal();
	const unsigned sizeOfData() { return sizeOf; }
	void setSize(const std::vector<unsigned> new_sz) { sz = new_sz; }

	/// <summary>
	/// Splits the data object into parts with at most 'size' size. If the source data object is not divisible into a chunks of 'size' without remainder, the last part will be smaller and contain the remaining objects. The original data is not duplicated or destroyed.
	/// </summary>
	/// <param name="size">Size of one data chunk. The last chunk might be smaller</param>
	/// <returns>Vector of data objects, containing pointers to parts the original data</returns>
	std::vector<Data*> sliceSize(unsigned size);
	/// <summary>
	/// Splits the data object into 'parts' pieces. If the source data object is not divisible into a number of 'parts' without remainder, the last part will be smaller and contain the remaining objects. The original data is not duplicated or destroyed.
	/// </summary>
	/// <param name="parts">Number of parts to split the data object into</param>
	/// <returns>Vector of data objects, containing pointers to parts the original data</returns>
	std::vector<Data*> sliceParts(unsigned parts);

	static std::vector<std::pair<std::string, long long unsigned>> getDurationDetails();

	int bcast_M_to_W();
	int bcast_W_to_W(const int source);
	int send_M_to_W(const int receiver, const int tag);
	int send_W_to_M(const int tag);
	int send_W_to_W(const int receiver, const int tag);
	MPI_Status recv_W_from_M(const int tag = MPI_ANY_TAG);
	MPI_Status recv_M_from_W(const int source = MPI_ANY_SOURCE, const int tag = MPI_ANY_TAG);
	MPI_Status recv_W_from_W(const int source = MPI_ANY_SOURCE, const int tag = MPI_ANY_TAG);
	int gather_W_to_M(void* result);
	int allGather_W_to_W(void* result);
	int reduce_W_to_M(MPI_Datatype datatype, MPI_Op op);
	int allReduce_W_to_W(void* result, MPI_Datatype datatype, MPI_Op op);

	static TAGS getLastTag() { return tag; }
	static void setLastTag(TAGS t) { tag = t; }
	static void setLastTag(int i) { tag = static_cast<TAGS>(i); }
	static Data::TAGS expectTerminate();

};
