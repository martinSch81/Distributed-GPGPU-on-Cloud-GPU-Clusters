// author: Schuchardt Martin, csap9442

#pragma once
#include "Node.h"

#include <vector>
#include <fstream>
#include <iostream>
#include <chrono>
#include <string>



extern const std::string CHECKPOINT_FILE;

class Checkpoint;
class Distributor;

class Checkpoint {

public:
	bool save(Distributor* distributor);
	bool read(Distributor* distributor);
};