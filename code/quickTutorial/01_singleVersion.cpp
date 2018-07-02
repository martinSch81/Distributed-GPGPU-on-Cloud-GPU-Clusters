// quick tutorial how to migrate single-server code to a distributed version
// author: Schuchardt Martin, csap9442

#include <iostream>
#include <string>

using namespace std;

const unsigned N = 27;

void init(int *input) {
	for (unsigned i = 0; i < N; ++i)
		input[i] = i;
}

void calc(int *input, int *result) {
	*result = 0;
	for (unsigned i = 0; i < N; ++i) {
		*result += input[i];
	}
}

void verify(int *input, int *result) {
	int should = 0;
	for (unsigned i = 0; i < N; ++i)
		should += input[i];

	if (*result == should) {
		cout << "OK: calculated " + to_string(*result) + ", expected " + to_string(should) << endl;
		return;
	}

	cerr << "ERROR: calculated " + to_string(*result) + ", expected " + to_string(should) << endl;
}


int main(int argc, char** argv) {
	int input[N];
	int result;

	init(input);
	calc(input, &result);
	verify(input, &result);

	return 0;
}