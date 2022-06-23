.PHONY : all clean

all : test

test : test.cpp ReadParser.hpp gzip_utils.hpp external/slw287r_trimadap/izlib.h external/kseq.h compress.hpp external/concurrentqueue.h external/thread_utils.hpp
	g++ --std=c++11 -O3 -Wall $< -o $@ -lisal -ldeflate -lpthread

clean :
	rm -f test

