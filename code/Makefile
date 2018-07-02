EXECUTABLES = alg01_gmres benchmark distributor mpiClusterTest mpiSample quickTutorial showDevices
ALLEXECUTABLES = $(EXECUTABLES)


.PHONY: build run clean

build: $(EXECUTABLES) Makefile
	@echo "*****************************  building ********************************"
	@(for file in ${EXECUTABLES}; do echo "**************** building $$file ****************"; make -C $$file; done)
	@echo "******************************** done **********************************"

run: $(EXECUTABLES) Makefile
	@echo "******************************  execute ***********************************"
	@(for file in ${EXECUTABLES}; do echo "**************** testing $$file ****************"; make -C $$file run; done)
	@echo "******************************** done *************************************"

clean:
	@(for file in ${EXECUTABLES}; do make -C $$file clean; done)
	rm -rf cmake-build-debug
