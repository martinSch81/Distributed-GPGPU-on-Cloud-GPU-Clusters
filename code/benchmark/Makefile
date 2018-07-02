EXECUTABLES = leibniz microBenchmark mmul
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
	@(for file in ${EXECUTABLES}; do make -C $$file clean; rm -f $$file/$$file\CL.c.csv; done)
	rm -f openCL.log
	rm -rf ./Debug ./x64 ./.vs
