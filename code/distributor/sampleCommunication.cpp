// Test for communication between Master/Worker and Worker/Worker
// author: Schuchardt Martin, csap9442

#include "Distributor.h"
#include "Checkpoint.h"

#include <iostream>
#include <algorithm>


using namespace std;
const unsigned MAX_ELEMENTS = 15346;


int kernel_m2w_bcast(Node &n, Distributor &d) {
	const unsigned M = *static_cast<unsigned*>(d.getArgument("M")->get());

	unsigned *x;
	try {
		x = new unsigned[M*M];
	} catch (const std::exception& e) {
		cerr << string(COLOR_RED) + d.whoAmI() + ": ERROR: could not allocate space for M*M=" + to_string(M) + "*" + to_string(M) + " elements:" + string(e.what()) + ".\nTerminating now." + string(COLOR_NC) << endl;
		return -2;
	}
	Data X(x, { M, M }, sizeof(unsigned));
	{
		if (d.isMaster()) {
			x[0] = 17;
			n.addOutput(indentLogText("bcasting to all workers..."));
			X.bcast_M_to_W();
		} else {
			X.bcast_M_to_W();
			n.addOutput(indentLogText("received bcast from master instance."));
			assert(x[0] == 17);
		}
		MPI_Barrier(MPI_COMM_CLUSTER);
	}
	delete[] x;
	return 0;
}

int kernel_m2w_p2p(Node &n, Distributor &d) {
	const unsigned N = *static_cast<unsigned*>(d.getArgument("N")->get());
	
	unsigned *x;
	try {
		x = new unsigned[N*N];
	} catch (const std::exception& e) {
		cerr << string(COLOR_RED) + d.whoAmI() + ": ERROR: could not allocate space for N*N=" + to_string(N) + "*" + to_string(N) + " elements:" + string(e.what()) + ".\nTerminating now." + string(COLOR_NC) << endl;
		return -2;
	}
	Data X(x, { N, N }, sizeof(unsigned));
	{
		int MPI_TAG_P2P = 816;
		if (d.isMaster()) {
			x[0] = 100;
			for (unsigned i = 0; i < d.getSize(); ++i) {
				x[0] = 100 + i;
				n.addOutput(indentLogText("sending p2p msg to worker (rank: " + to_string(i) + ")"));
				X.send_M_to_W(i, MPI_TAG_P2P);
			}
			for (unsigned i = 0; i < d.getSize(); ++i) {
				auto status = X.recv_M_from_W();
				n.addOutput(indentLogText("received p2p msg from worker (rank: " + to_string(status.MPI_SOURCE) + ")"));
				assert(x[0] == (unsigned)(101 + status.MPI_SOURCE));
			}
		} else {
			X.recv_W_from_M();
			n.addOutput(indentLogText("received p2p msg from master instance"));
			assert(x[0] == 100 + d.getRank());
			++x[0];
			n.addOutput(indentLogText("sending p2p msg to master instance"));
			X.send_W_to_M(MPI_TAG_P2P);
		}
		MPI_Barrier(MPI_COMM_CLUSTER);
	}
	delete[] x;
	return 0;
}

int kernel_w2w_bcast(Node &n, Distributor &d) {
	const unsigned O = *static_cast<unsigned*>(d.getArgument("O")->get());

	unsigned *x;
	try {
		x = new unsigned[O*O];
	} catch (const std::exception& e) {
		cerr << string(COLOR_RED) + d.whoAmI() + ": ERROR: could not allocate space for O*O=" + to_string(O) + "*" + to_string(O) + " elements:" + string(e.what()) + ".\nTerminating now." + string(COLOR_NC) << endl;
		return -2;
	}
	Data X(x, { O, O }, sizeof(unsigned));
	{
		if (d.getRank() == 0) {
			x[0] = 17;
			n.addOutput(indentLogText("bcasting to all other workers..."));
			X.bcast_W_to_W(d.getRank());
		} else {
			X.bcast_W_to_W(0);
			n.addOutput(indentLogText("received bcast from W0."));
			assert(x[0] == 17);
		}
	}
	delete[] x;
	MPI_Barrier(MPI_COMM_WORKER_TO_WORKER);
	return 0;
}

int kernel_w2w_p2p(Node &n, Distributor &d) {
	const unsigned P = *static_cast<unsigned*>(d.getArgument("P")->get());
	unsigned *x;
	try {
		x = new unsigned[P*P];
	} catch (const std::exception& e) {
		cerr << string(COLOR_RED) + d.whoAmI() + ": ERROR: could not allocate space for P*P=" + to_string(P) + "*" + to_string(P) + " elements:" + string(e.what()) + ".\nTerminating now." + string(COLOR_NC)<< endl;
		return -2;
	}
	Data X(x, { P, P }, sizeof(unsigned));
	{
		int MPI_TAG_P2P = 816;
		if (d.getRank() == 0) {
			x[0] = 1;
			n.addOutput(indentLogText("sending p2p msg to next worker (rank: " + to_string(d.getRank() + 1) + ")"));
			X.send_W_to_W(d.getRank() + 1, MPI_TAG_P2P);
			X.recv_W_from_W(d.getSize() - 1);
			n.addOutput(indentLogText("received p2p msg from previous worker (rank: " + to_string(d.getSize() - 1) + ")"));
			assert(x[0] == d.getSize());
		} else {
			X.recv_W_from_W((d.getRank() - 1));
			n.addOutput(indentLogText("received p2p msg from previous worker (rank: " + to_string((d.getRank() - 1) % d.getSize()) + ")"));
			assert(x[0] == ((d.getRank() - 1) % d.getSize() + 1));
			++x[0];
			n.addOutput(indentLogText("sending p2p msg to next worker (rank: " + to_string((d.getRank() + 1) % d.getSize()) + ")"));
			X.send_W_to_W((d.getRank() + 1) % d.getSize(), MPI_TAG_P2P);
		}
	}
	delete[] x;
	MPI_Barrier(MPI_COMM_WORKER_TO_WORKER);
	return 0;
}

int kernel_w2m_gather(Node &n, Distributor &d) {
	const unsigned Q = *static_cast<unsigned*>(d.getArgument("Q")->get());
	{
		if (d.isMaster()) {
			int* x = new int[d.getSize() * Q*Q];
			Data X(x, { d.getSize() * Q*Q }, sizeof(int));

			n.addOutput(indentLogText("gathering data from workers at master..."));
			int* y = new int[d.getSize() * Q*Q];
			X.gather_W_to_M(y);
			n.addOutput(indentLogText("...done"));
			
			for (unsigned i = 0; i < d.getSize(); ++i) {
				for (unsigned j = 0; j < Q*Q; ++j)
					if (y[i*Q*Q + j] != (int)i)
						assert(false);
			}
			delete[] x;
		} else {
			int* x = new int[Q*Q];
			Data X(x, { Q*Q }, sizeof(int));
			for (unsigned i = 0; i < Q*Q; ++i)
				x[i] = d.getRank();

			n.addOutput(indentLogText("sending data to be gathered at master..."));
			int* y = new int[d.getSize() * Q*Q];
			X.gather_W_to_M(y);
			n.addOutput(indentLogText("...done"));
			delete[] x;
		}
	}
	MPI_Barrier(MPI_COMM_CLUSTER);
	return 0;
}

int kernel_w2w_allGather(Node &n, Distributor &d) {
	const unsigned R = *static_cast<unsigned*>(d.getArgument("R")->get());

	int* x = new int[d.getSize() * R*R];
	Data X(x, { d.getSize() * R*R }, sizeof(int));
	for (unsigned i = 0; i < R*R; ++i)
		x[i] = d.getRank();

	n.addOutput(indentLogText("all-gathering data from workers at all workers..."));

	int* y = new int[d.getSize() * R*R];
	X.allGather_W_to_W(y);
	
	n.addOutput(indentLogText("...done"));

	for (unsigned i = 0; i < d.getSize(); ++i) {
		for (unsigned j = 0; j < R*R; ++j)
			if (y[i*R*R + j] != (int)i) {
				assert(false);
			}
	}
	delete[] x;
	delete[] y;

	MPI_Barrier(MPI_COMM_WORKER_TO_WORKER);
	return 0;
}

int kernel_w2m_reduce(Node &n, Distributor &d) {
	const unsigned S = *static_cast<unsigned*>(d.getArgument("S")->get());
	{
		if (d.isMaster()) {
			int* x = new int[S*S];
			Data X(x, { S*S }, sizeof(int));

			n.addOutput(indentLogText("reducing(+) data from workers at master..."));
			X.reduce_W_to_M(MPI_INT, MPI_SUM);
			n.addOutput(indentLogText("...done"));

			int sum = 0;
			for (unsigned i = 0; i < d.getSize(); ++i)
				sum += i;
			
			for (unsigned i = 0; i < S*S; ++i)
				if (x[i] != sum)
					assert(false);
			delete[] x;
		} else {
			int* x = new int[S*S];
			Data X(x, { S*S }, sizeof(int));
			for (unsigned i = 0; i < S*S; ++i)
				x[i] = d.getRank();

			n.addOutput(indentLogText("sending data to be reduced(+) at master..."));
			X.reduce_W_to_M(MPI_INT, MPI_SUM);
			n.addOutput(indentLogText("...done"));
			delete[] x;
		}
	}
	MPI_Barrier(MPI_COMM_CLUSTER);
	return 0;
}
int kernel_w2w_allReduce(Node &n, Distributor &d) {
	const unsigned T = *static_cast<unsigned*>(d.getArgument("T")->get());

	int* x = new int[T*T];
	Data X(x, { T*T }, sizeof(int));
	for (unsigned i = 0; i < T*T; ++i)
		x[i] = d.getRank();

	n.addOutput(indentLogText("all-reduce(+) data from workers at all workers..."));

	int* y = new int[T*T];
	X.allReduce_W_to_W(y, MPI_INT, MPI_SUM);

	n.addOutput(indentLogText("...done"));

	int sum = 0;
	for (unsigned i = 0; i < d.getSize(); ++i)
		sum += i;

	for (unsigned i = 0; i < T*T; ++i)
		if (y[i] != sum)
			assert(false);

	delete[] x;
	delete[] y;

	MPI_Barrier(MPI_COMM_WORKER_TO_WORKER);
	return 0;
}



int main(int argc, char** argv) {
	unsigned N = 727; // small size for fast testing, 15000 is maximum

	string arg;
	unsigned M = N, O = N, P = N, Q = N, R = N, S = N, T = N;
	if (parseArguments(argc, argv, "M=", arg) >= 0)
		M = atoi(arg.c_str());
	if (parseArguments(argc, argv, ARGUMENT_N, arg) >= 0)
		N = atoi(arg.c_str());
	if (parseArguments(argc, argv, "O=", arg) >= 0)
		O = atoi(arg.c_str());
	if (parseArguments(argc, argv, "P=", arg) >= 0)
		P = atoi(arg.c_str());
	if (parseArguments(argc, argv, "Q=", arg) >= 0)
		Q = atoi(arg.c_str());
	if (parseArguments(argc, argv, "R=", arg) >= 0)
		R = atoi(arg.c_str());
	if (parseArguments(argc, argv, "S=", arg) >= 0)
		S = atoi(arg.c_str());
	if (parseArguments(argc, argv, "T=", arg) >= 0)
		T = atoi(arg.c_str());

	string mpiVersion;
	if (!getMPI_StandardVersion(1, 6, mpiVersion)) {
		cerr << string(COLOR_RED) + "ERROR: incompatible MPI version " + mpiVersion + " found, expecting at least v1.6. Terminating now." + string(COLOR_NC) << endl;
		exit(EXIT_FAILURE);
	}

	Node *m2w_bcast =     new Node("master-to-worker bcast    ", kernel_m2w_bcast, kernel_m2w_bcast);
	Node *m2w_p2p =       new Node("master-to-worker p2p      ", kernel_m2w_p2p, kernel_m2w_p2p);
	Node *w2w_bcast =     new Node("worker-to-worker bcast    ", nullptr, kernel_w2w_bcast);
	Node *w2w_p2p =       new Node("worker-to-worker p2p      ", nullptr, kernel_w2w_p2p);
	Node *w2m_gather =    new Node("worker-to-master gather   ", kernel_w2m_gather, kernel_w2m_gather);
	Node *w2w_allGather = new Node("worker-to-worker allGather", nullptr, kernel_w2w_allGather);
	Node *w2m_reduce =    new Node("worker-to-master reduce   ", kernel_w2m_reduce, kernel_w2m_reduce);
	Node *w2w_allReduce = new Node("worker-to-worker allReduce", nullptr, kernel_w2w_allReduce);
	Node *shutdown =      new Node("shutdown workers          ", Distributor::kernel_shutdown, Distributor::kernel_shutdown);
	m2w_p2p->addDependency(m2w_bcast);
	w2w_bcast->addDependency(m2w_p2p);
	w2w_p2p->addDependency(w2w_bcast);
	w2m_gather->addDependency(w2w_p2p);
	w2w_allGather->addDependency(w2m_gather);
	w2m_reduce->addDependency(w2w_allGather);
	w2w_allReduce->addDependency(w2m_reduce);
	shutdown->addDependency(w2w_allReduce);

	Distributor d(argc, argv, shutdown);

	Data N_(&N, { 1 }, sizeof(unsigned));
	Data M_(&M, { 1 }, sizeof(unsigned));
	Data O_(&O, { 1 }, sizeof(unsigned));
	Data P_(&P, { 1 }, sizeof(unsigned));
	Data Q_(&Q, { 1 }, sizeof(unsigned));
	Data R_(&R, { 1 }, sizeof(unsigned));
	Data S_(&S, { 1 }, sizeof(unsigned));
	Data T_(&T, { 1 }, sizeof(unsigned));
	d.addArguments({ { "N", &N_ } });
	d.addArguments({ { "M", &M_ } });
	d.addArguments({ { "O", &O_ } });
	d.addArguments({ { "P", &P_ } });
	d.addArguments({ { "Q", &Q_ } });
	d.addArguments({ { "R", &R_ } });
	d.addArguments({ { "S", &S_ } });
	d.addArguments({ { "T", &T_ } });

	if (d.getSize() < 2) {
		cerr << string(COLOR_RED) + "ERROR: at least 2 workers are required, this cluster has only " + to_string(d.getSize()) + string(COLOR_NC) << endl;
		exit(EXIT_FAILURE);
	}
	
	if (d.isMaster()) {
		string mpiVersion;
		getMPI_StandardVersion(0, 0, mpiVersion);
		cout << string(COLOR_YELLOW) + "  MPI(v" << mpiVersion << ") cluster size: " << d.getSize() << endl;
		cout << "  Simple communication tests using" << endl;
		cout << "         M=" + to_string(M) + ",\t -> M*M=" + to_string(M*M) + " elements for bcast master -> worker," << endl;
		cout << "         N=" + to_string(N) + ",\t -> N*N=" + to_string(N*N) + " elements for send/recv master <-> worker" << endl;
		cout << "         O=" + to_string(O) + ",\t -> O*O=" + to_string(O*O) + " elements for bcast worker -> worker" << endl;
		cout << "         P=" + to_string(P) + ",\t -> P*P=" + to_string(P*P) + " elements for send/recv worker <-> worker" << endl;
		cout << "         Q=" + to_string(Q) + ",\t -> Q*Q=" + to_string(Q*Q) + " elements for gather worker <-> worker" << endl;
		cout << "         R=" + to_string(R) + ",\t -> R*R=" + to_string(R*R) + " elements for all gather worker <-> worker" << endl;
		cout << "         S=" + to_string(S) + ",\t -> S*S=" + to_string(S*S) + " elements for reduce worker <-> worker" << endl;
		cout << "         T=" + to_string(T) + ",\t -> T*T=" + to_string(T*T) + " elements for all reduce worker <-> worker" << endl << endl << string(COLOR_NC);
	}

	if (d.getSize() < 1) {
		cerr << string(COLOR_RED) + "ERROR: communication test needs at least 2 nodes; one master and [1...N] workers" + string(COLOR_NC) << endl;
		exit(EXIT_FAILURE);
	}
	int err = d.run();


	if (d.isMaster() && !d.isRestarting()) {
		cout << string(COLOR_GREEN) + "  elapsed time for communication test: \t" << to_string(d.getDuration()) << "  ms" + string(COLOR_NC) << endl;
		if (err)
			cerr << string(COLOR_RED) + "ERROR: finished with RC=" + to_string(err) + string(COLOR_NC) << endl << std::string(80, '*') << endl << endl << endl;
		else
			cout << string(COLOR_GREEN) + "finished with RC=" + to_string(err) + string(COLOR_NC) << endl << std::string(80, '*') << endl;
	}
	if (!d.isRestarting()) {
		writeCSV(genCSVFileName(argv[0], d.getRank()), { pair<string, unsigned>("num_gpus", Distributor::getNumGPUs()), pair<string, unsigned>("cluster_size", d.getSize()), pair<string, unsigned>("N", N), pair<string, unsigned>("M", M), pair<string, unsigned>("O", O), pair<string, unsigned>("P", P), pair<string, unsigned>("Q", Q), pair<string, unsigned>("R", R), pair<string, unsigned>("S", S), pair<string, unsigned>("T", T) }, d.getDurationDetails());
		for (auto detail : d.getDurationDetails())
			cout << "  " + detail.first + ": " + to_string(detail.second) << endl;
		cout << std::string(80, '*') << endl;
	}
	
	delete m2w_bcast;
	delete m2w_p2p;
	delete w2w_bcast;
	delete w2w_p2p;
	delete w2m_gather;
	delete w2w_allGather;
	delete w2m_reduce;
	delete w2w_allReduce;
	delete shutdown;
	
	exit (err);
}