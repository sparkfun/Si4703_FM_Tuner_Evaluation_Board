
source_files=examples/Radio.cpp \
		src/SparkFunSi4703.cpp \
		src/SparkFunSi4703.h

Radio: ${source_files} Makefile
	g++ -std=gnu++11 -lpthread -o Radio examples/Radio.cpp src/SparkFunSi4703.cpp -lwiringPi

.PHONY: clean
clean:
	rm -f Radio

.PHONY: run
run: Radio
	sudo ./Radio
