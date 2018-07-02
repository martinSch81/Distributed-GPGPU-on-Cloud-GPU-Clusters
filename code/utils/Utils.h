// author: Schuchardt Martin, csap9442

#pragma once

#ifdef _MSC_VER // Visual Studio
#define __PRETTY_FUNCTION__ __FUNCSIG__
#define popen _popen
#define pclose _pclose
#endif


#include <string>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <array>
#include <mpi.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <limits>
#include <thread>


#define WORKDIR "/tmp"

#ifdef __linux
#include <unistd.h>
#define INIT_HOSTNAME_FOR_WINDOWS {}

#define GET_PID getpid()

#else
#pragma comment(lib, "ws2_32.lib")
#define NOMINMAX
#include <WinSock2.h>
#define INIT_HOSTNAME_FOR_WINDOWS {WORD wVersionRequested = MAKEWORD(2, 2); WSADATA wsaData; WSAStartup(wVersionRequested, &wsaData);}

#define GET_PID GetCurrentProcessId()

#endif

extern std::string MPI_HOSTFILE_USAGE;

const char COLOR_RED[]    = "\033[0;31m";
const char COLOR_BLUE[]   = "\033[0;34m";
const char COLOR_GREEN[]  = "\033[0;32m";
const char COLOR_PURPLE[] = "\033[0;35m";
const char COLOR_YELLOW[] = "\033[0;33m";
const char COLOR_WHITE[]  = "\033[0;37m";
const char COLOR_BLACK[]  = "\033[0;30m";
const char COLOR_NC[]     = "\033[0m"; // No Color

extern std::string mpiInstanceName;
std::string getName();
void storeMPIInstanceName();
bool getMPI_StandardVersion(int minMajorVersion, int minMinorVersion, std::string &output);
std::string nowToString();
std::string nowShortToString();
std::string indentLogText(std::string output);

std::string getExecutableFullName(const std::string executable);
std::string getExecutableBaseName(const std::string executable);
std::string getDirectory(std::string fileName);

std::string ltrim(const std::string strng);
std::string rtrim(const std::string strng);
std::string trim(const std::string strng);

/// <summary>counts the number of MPI processes to be created as result. Optionally (param 2): returns NUM_GPUs; Optionally (param 3, 4) returns hostfile, splitted in new hostfile that fulfills new mpi size 'min_size'; Optionally (param 5, 6) returns if additional workers will run on master.
/// 
/// </summary>
/// <param name="mpiHostfile">path to hostfile</param>
/// <param name="num_GPUs">number of GPUs per worker, needs to be identical on all worker. If master operates as additional worker, then for master apply different rules (buggy behaviour, DANGEROUS but still too useful to ignore)</param>
/// <param name="hostfileParts">first entry are lines for new hostfile (fulfilles max_size), second is list of superfluous hosts for terminate.</param>
/// <param name="max_size">trigger to split cluster into two halves</param>
/// <param name="masterComputes">Optional parameter: boolean if master host has been found in mpi-hostfile. (master host may appear only once, only as first entry, and only without 'slots=XX', contrary to worker nodes who should use 'slots=XX')</param>
/// <param name="masterName">Optional parameter: name of master to search for in mpi.hostfile.</param>
/// <returns>the number of mpi processes to spawn regarding to current hostfile. Greater 0 for a valid hostfiles. For 0 or less see MPI_HOSTFILE_USAGE for explanations.</returns>
int splitHostfile(std::string mpiHostfile, int *num_GPUs, std::vector<std::vector<std::string>> *result, int *new_size, bool *masterComputes);
int parseArguments(const int argc, char** argv, const std::string search, std::string &result);

std::string genCSVFileName(std::string sourceFileName, const int rank);
void writeCSV(std::string csvFile, std::vector<std::pair<std::string, unsigned>> args, std::vector<std::pair<std::string, unsigned long long>> results);
void mpiDebugHook(const int myRank, const int requestedRank, const MPI_Comm comm);
