// author: Schuchardt Martin, csap9442

#define _CRT_SECURE_NO_WARNINGS
#define __STDC_WANT_LIB_EXT1__ 1

#include "Utils.h"

std::string mpiInstanceName;


std::string nowToString() {
	std::time_t end_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	std::string result = ctime(&end_time);
	result.erase(std::remove(result.begin(), result.end(), '\n'), result.end());
	return result;
}

std::string nowShortToString() {
	std::time_t t = std::time(NULL);
	char mbstr[100];
	std::strftime(mbstr, sizeof(mbstr), "%Y%m%d_%H%M%S", std::localtime(&t));

	return std::string(mbstr);
}

std::string getExecutableFullName(const std::string executable) {
	std::string baseName(executable);

	auto pos = executable.rfind("/");
	if (pos != std::string::npos)
		baseName = executable.substr(pos + 1);

	std::string remoteExecutable(WORKDIR);
	remoteExecutable.append('/' + baseName);

	return remoteExecutable;
}

std::string getExecutableBaseName(const std::string executable) {
	auto startPos = executable.find_last_of("\\/");
	if (startPos == std::string::npos || startPos == executable.length()) {
		return "";
	}

	return executable.substr(startPos + 1);
}

std::string getDirectory(std::string fileName) {
	size_t found = fileName.find_last_of("/\\");
	if (found == std::numeric_limits<std::size_t>::max())
		return ".";
	else
		return fileName.substr(0, found);
}

bool getMPI_StandardVersion(int minMajorVersion, int minMinorVersion, std::string &output) {
	int ver, subver;
	MPI_Get_version(&ver, &subver);
	output = std::to_string(ver) + "." + std::to_string(subver);
	if (ver > minMajorVersion)
		return true;
	if (ver == minMajorVersion && subver >= minMinorVersion)
		return true;

	return false;
}

std::string getName() { return mpiInstanceName; };

void storeMPIInstanceName() {
	char name[MPI_MAX_PROCESSOR_NAME];
	int len = 0;

	int flag;
	MPI_Initialized(&flag);
	if (!flag)
		throw "mpi_not_initialized";

	MPI_Get_processor_name(name, &len);
	mpiInstanceName = name;
}


std::string ltrim(const std::string strng) {
	size_t ltrim = strng.find_first_not_of(" \t\f\v\n\r");
	if (std::string::npos != ltrim) {
		return strng.substr(ltrim);
	}
	return strng;
}

std::string rtrim(const std::string strng) {
	size_t rtrim = strng.find_last_not_of(" \t\f\v\n\r");
	if (std::string::npos != rtrim) {
		return strng.substr(0, rtrim + 1);
	}
	return strng;
}

std::string trim(const std::string strng) {
	return ltrim(rtrim(strng));
}

std::string MPI_HOSTFILE_USAGE("  Use mpi hostfile as follows (find flaws indicated by preceding RC):\n\
    0: number of total mpi process on all instances is 0; no valid content in mpi.hostfile (ERROR);\n\
    -1: could not read mpi.hostfile;\n\
    -3: master node had 'slots=1' defined, needs at least '2';\n\
    -4: inconsisten number of 'slots=XX' (no 'slots' means 1, master node may not have no 'slots' parameter since slots needs to be at least '2');\n\
    -5: master node needs to be the first host entry in mpi.hostfile, if it should be used as a worker. Cluster resizing will start with splitting nodes from the end of the list, therfor the master needs to be the first entry.");

int splitHostfile(std::string mpiHostfile, int *num_GPUs, std::vector<std::vector<std::string>> *hostfileParts, int *new_size, bool *masterComputes) {
	if (new_size)
	if (hostfileParts) {
		hostfileParts->clear();
		hostfileParts->push_back(std::vector<std::string>()); // all nodes including rank <= max_size
		hostfileParts->push_back(std::vector<std::string>()); // all nodes with rank > max_size
	}
	std::string masterName = getName();

	int max_size = std::numeric_limits<int>::max();
	if (new_size)
		max_size = *new_size;

	bool masterComputesTmp = false;

	// read stream into vector (skip all commented-out nonsense)
	std::vector<std::string> lines;
	std::ifstream fileStream(mpiHostfile);
	if (fileStream) {
		std::string line;
		while (std::getline(fileStream, line)) {
			line = ltrim(line);
			if (line == "")
				continue;

			if (line.front() == '#')
				continue;
			lines.push_back(line);
		}
		fileStream.close();
	} else
		return -1;

	// find num_gpus
	std::string search("slots=");
	int num_GPUs_tmp = 0;
	for (unsigned i = 0; i < lines.size(); ++i) {
		auto line = lines[i];
		int numGPU = -1;

		auto offset = line.find(search, 0);
		if (offset != std::string::npos) {
			numGPU = stoi(line.substr(offset + search.length()));
		} else {
			numGPU = 1;
		}

		// entry for master must have slots=XX entry one higher than the other workers
		auto host = trim(line.substr(0, offset));
		if (host == masterName) {
			masterComputesTmp = true;
			if (i != 0)
				return -5;
			--numGPU; // 1 process already runs on master, so one less additional workers can be launched
			if (numGPU < 1)
				return -3;
		}

		if (num_GPUs_tmp == 0) // in the first round, if no 'slots=XX' has been found, just define num_GPUs with the value of the first line
			num_GPUs_tmp = numGPU;
		else if (num_GPUs_tmp != numGPU) // later, if currently parsed and stored value mismatch -> ERROR
			return -4;
	}

	// do splitting
	int new_size_ceil = 0;
	int countingSlots = 0;
	for (auto line : lines) {
		countingSlots += num_GPUs_tmp;

		auto offset = line.find(search, 0);
		auto hostname = trim(line.substr(0, offset));
		if (new_size_ceil < max_size) {
			if (hostfileParts)
				(*hostfileParts)[0].push_back(line);
			new_size_ceil = countingSlots;
		} else
			if (hostfileParts)
				(*hostfileParts)[1].push_back(hostname);
	}

	if (num_GPUs)
		*num_GPUs = num_GPUs_tmp;
	if (new_size)
		*new_size = new_size_ceil;
	if (masterComputes)
		*masterComputes = masterComputesTmp;

	return countingSlots;
}

std::string indentLogText(std::string input) {
	static size_t LOG_PREFIX_LENGTH = 2 + nowToString().length() + 2; // eg '  Fri Apr 28 11:21:26 2017: '

	std::string result;
	std::istringstream iss(input);

	for (std::string line; std::getline(iss, line); )
		result += std::string(LOG_PREFIX_LENGTH, ' ') + line + "\n";

	return result;
}

int parseArguments(const int argc, char** argv, const std::string search, std::string &result) {
	for (int i = 0; i < argc; ++i) {
		std::string arg = argv[i];
		if (arg.substr(0, search.length()) == search) {
			result = arg.substr(search.length());
			return i;
		}
	}

	return -1;
}

std::string genCSVFileName(std::string sourceFileName, const int rank) {
	std::string csvFile = getExecutableBaseName(sourceFileName);
	return csvFile.substr(0, csvFile.find_last_of('.')) + "." + std::to_string(rank) + "." + getName() + ".csv";
}

void writeCSV(std::string csvFile, std::vector<std::pair<std::string, unsigned>> args, std::vector<std::pair<std::string, unsigned long long>> results) {
	const char DELIMITER = ';';
	std::ifstream ifs(csvFile);
	ifs.close();
	if (!ifs.good()) {
		std::ofstream ofs;
		ofs.open(csvFile, std::ofstream::out);
		bool once = true;
		for (auto entry : args) {
			if (once)
				once = false;
			else
				ofs << DELIMITER;
			ofs << entry.first;

		}
		for (auto entry : results) {
			if (once)
				once = false;
			else
				ofs << DELIMITER;
			ofs << entry.first;
		}
		ofs << std::endl;
		ofs.close();
	}

	std::ofstream ofs;
	ofs.open(csvFile, std::ofstream::out | std::ofstream::app);
	bool once = true;
	for (auto entry : args) {
		if (once)
			once = false;
		else
			ofs << DELIMITER;
		ofs << entry.second;
	}
	for (auto entry : results) {
		if (once)
			once = false;
		else
			ofs << DELIMITER;
		ofs << entry.second;
	}
	ofs << std::endl;
	ofs.close();
}

void mpiDebugHook(const int myRank, const int requestedRank, const MPI_Comm comm) {
	if (myRank == requestedRank) {
		std::cout << "PID " + std::to_string(GET_PID) + " on " + getName() + " ready for attach" << std::endl;
		volatile int i = 0;
		while (0 == i)
			std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	MPI_Barrier(comm);
}
