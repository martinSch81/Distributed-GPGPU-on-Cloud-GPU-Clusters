// author: Schuchardt Martin, csap9442

#include "Checkpoint.h"
#include "Distributor.h"


const std::string CHECKPOINT_FILE("checkpoint.sav");

void saveArgument(std::pair<const std::string, Data*> &param, std::ofstream &ofs) {
	size_t key_length = param.first.length();
	ofs.write((char*)&key_length, sizeof(size_t));
	ofs.write((char*)param.first.c_str(), key_length * sizeof(char));

	size_t data_size_vector = param.second->size().size();
	ofs.write((char*)&data_size_vector, sizeof(data_size_vector));
	unsigned sizeTotal = 1;
	for (auto size : param.second->size()) {
		sizeTotal *= size;
	}
	ofs.write((char*)&param.second->size()[0], data_size_vector * sizeof(unsigned));

	unsigned data_size_t = param.second->sizeOfData();
	ofs.write((char*)&data_size_t, sizeof(data_size_t));

	void *data = param.second->get();
	ofs.write((char*)data, sizeTotal*data_size_t);
}

std::pair<char*, Data*> readArgument(std::ifstream &ifs) {
	size_t key_length;
	ifs.read((char*)&key_length, sizeof(size_t));

	char* key = new char[key_length + 1];
	ifs.read((char*)key, key_length * sizeof(char));
	key[key_length] = '\0';

	size_t data_size_vector;
	ifs.read((char*)&data_size_vector, sizeof(data_size_vector));

	std::vector<unsigned> sizes;
	sizes.resize(data_size_vector);

	unsigned sizeTotal = 1;
	ifs.read((char*)&sizes[0], data_size_vector * sizeof(unsigned));
	for (auto size : sizes) {
		sizeTotal *= size;
	}

	unsigned data_size_t;
	ifs.read((char*)&data_size_t, sizeof(data_size_t));

	char *data = new char[sizeTotal*data_size_t];
	ifs.read((char*)data, sizeTotal*data_size_t);

	Data* newData = new Data(data, sizes, data_size_t);

	return std::pair<char*, Data*>(key, newData);
}

bool Checkpoint::save(Distributor *distributor) { // IMPLEMENT use save methods from used classes
	assert(distributor->targetNode);  // for save and read -> fail if targetNode == nullptr, since this means allNodes is not defined yet

	std::ofstream ofs(CHECKPOINT_FILE + "." + std::to_string(distributor->MPI_RANK), std::ios::binary);	

	size_t dotGraph_length = distributor->dotGraph.length();
	ofs.write((char*) &dotGraph_length, sizeof(size_t));
	ofs.write((char*) distributor->dotGraph.c_str(), dotGraph_length * sizeof(char));

	distributor->stdOutStringStream << distributor->getOutput();
	size_t stdOutStringStream_length = distributor->stdOutStringStream.str().length();
	ofs.write((char*)&stdOutStringStream_length, sizeof(size_t));
	ofs.write((char*)distributor->stdOutStringStream.str().c_str(), stdOutStringStream_length * sizeof(char));
	
	size_t stdErrStringStream_length = distributor->stdErrStringStream.str().length();
	ofs.write((char*)&stdErrStringStream_length, sizeof(size_t));
	ofs.write((char*)distributor->stdErrStringStream.str().c_str(), stdErrStringStream_length * sizeof(char));

	ofs.write((char*)&distributor->start, sizeof(distributor->start));
	ofs.write((char*)&distributor->clusterCreation, sizeof(distributor->clusterCreation));
	ofs.write((char*)&distributor->cluster_restart_block_time_unit, sizeof(distributor->cluster_restart_block_time_unit));
	
	ofs.write((char*)&Data::duration_bcast_M_to_W, sizeof(Data::duration_bcast_M_to_W));
	ofs.write((char*)&Data::duration_bcast_W_to_W, sizeof(Data::duration_bcast_W_to_W));
	ofs.write((char*)&Data::duration_send_M_to_W, sizeof(Data::duration_send_M_to_W));
	ofs.write((char*)&Data::duration_send_W_to_M, sizeof(Data::duration_send_W_to_M));
	ofs.write((char*)&Data::duration_send_W_to_W, sizeof(Data::duration_send_W_to_W));
	ofs.write((char*)&Data::duration_recv_W_from_M, sizeof(Data::duration_recv_W_from_M));
	ofs.write((char*)&Data::duration_recv_M_from_W, sizeof(Data::duration_recv_M_from_W));
	ofs.write((char*)&Data::duration_recv_W_from_W, sizeof(Data::duration_recv_W_from_W));

	size_t arguments_size = distributor->getArguments().size();
	ofs.write((char*)&arguments_size, sizeof(size_t));
	for (auto param : distributor->getArguments()) {
		saveArgument(param, ofs);
	}

	for (auto node : distributor->allNodes) {
		ofs.write((char*)&node->start, sizeof(node->start));
		ofs.write((char*)&node->end, sizeof(node->end));

		ofs.write((char*)&node->finished, sizeof(bool));

		ofs.write((char*)&node->LOOP_COUNTER, sizeof(node->LOOP_COUNTER));
		ofs.write((char*)&node->TOTAL_COUNT, sizeof(node->TOTAL_COUNT));

		arguments_size = node->getArguments().size();
		ofs.write((char*)&arguments_size, sizeof(size_t));

		for (auto param : node->getArguments())
			saveArgument(param, ofs);
	}

	ofs.close();
	distributor->restartingCluster = true;
	return ofs.good();
}

bool Checkpoint::read(Distributor *distributor) {
	assert(distributor->targetNode);  // for save and read -> fail if targetNode == nullptr, since this means allNodes is not defined yet

	std::ifstream ifs(CHECKPOINT_FILE + "." + std::to_string(distributor->MPI_RANK), std::ios_base::binary);
	if (ifs.good()) {
		size_t dotGraph_length;
		ifs.read((char*)&dotGraph_length, sizeof(size_t));
		distributor->dotGraph.resize(dotGraph_length);
		ifs.read((char*) &distributor->dotGraph[0], dotGraph_length * sizeof(char));

		size_t stdOutStringStream_length;
		ifs.read((char*)&stdOutStringStream_length, sizeof(size_t));
		std::string stdOutStringStreamBuffer;
		stdOutStringStreamBuffer.resize(stdOutStringStream_length);
		ifs.read((char*)&stdOutStringStreamBuffer[0], stdOutStringStream_length * sizeof(char));
		distributor->stdOutStringStream.str("");
		distributor->stdOutStringStream << stdOutStringStreamBuffer << std::endl;

		size_t stdErrStringStream_length;
		ifs.read((char*)&stdErrStringStream_length, sizeof(size_t));
		std::string stdErrStringStreamBuffer;
		stdErrStringStreamBuffer.resize(stdErrStringStream_length);
		ifs.read((char*)&stdErrStringStreamBuffer[0], stdErrStringStream_length * sizeof(char));
		distributor->stdErrStringStream.str("");
		distributor->stdErrStringStream << stdErrStringStreamBuffer;

		ifs.read((char*)&distributor->start, sizeof(distributor->start));
		ifs.read((char*)&distributor->clusterCreation, sizeof(distributor->clusterCreation));
		ifs.read((char*)&distributor->cluster_restart_block_time_unit, sizeof(distributor->cluster_restart_block_time_unit));

		ifs.read((char*)&Data::duration_bcast_M_to_W, sizeof(Data::duration_bcast_M_to_W));
		ifs.read((char*)&Data::duration_bcast_W_to_W, sizeof(Data::duration_bcast_W_to_W));
		ifs.read((char*)&Data::duration_send_M_to_W, sizeof(Data::duration_send_M_to_W));
		ifs.read((char*)&Data::duration_send_W_to_M, sizeof(Data::duration_send_W_to_M));
		ifs.read((char*)&Data::duration_send_W_to_W, sizeof(Data::duration_send_W_to_W));
		ifs.read((char*)&Data::duration_recv_W_from_M, sizeof(Data::duration_recv_W_from_M));
		ifs.read((char*)&Data::duration_recv_M_from_W, sizeof(Data::duration_recv_M_from_W));
		ifs.read((char*)&Data::duration_recv_W_from_W, sizeof(Data::duration_recv_W_from_W));

		size_t arguments_size;
		ifs.read((char*)&arguments_size, sizeof(size_t));
		for (size_t i = 0; i < arguments_size; ++i) {
			auto newData = readArgument(ifs);
			distributor->addArgument(newData.first, newData.second);
			delete newData.first;
		}

		for (auto node : distributor->allNodes) {
			ifs.read((char*)&node->start, sizeof(node->start));
			ifs.read((char*)&node->end, sizeof(node->end));

			ifs.read((char*)&node->finished, sizeof(bool));

			ifs.read((char*)&node->LOOP_COUNTER, sizeof(node->LOOP_COUNTER));
			ifs.read((char*)&node->TOTAL_COUNT, sizeof(node->TOTAL_COUNT));

			ifs.read((char*)&arguments_size, sizeof(size_t));
			for (size_t j = 0; j < arguments_size; ++j) {
				auto newData = readArgument(ifs);
				node->addArgument(newData.first, newData.second);
				delete newData.first;
			}
		}

		std::remove((CHECKPOINT_FILE + "." + std::to_string(distributor->MPI_RANK)).c_str());
		return ifs.good();
	}

	return true; // no checkpoint found, thats OK too.
}
