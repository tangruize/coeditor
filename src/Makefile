include ../Makefile.inc

SRC = $(wildcard *.cpp)

SRC_NO_MAIN = $(shell find . -type f -name '*.cpp' ! -name main.cpp)

EXE = ../coeditor

LIB_DIR = ../lib

LIBS = $(wildcard ${LIB_DIR}/*.so)

EXE_CXXFLAGS := ${CXXFLAGS} -Wl,-rpath,'$$ORIGIN'/lib

all : ${EXE}

${EXE} : main.o ${LIBS} $(SRC_NO_MAIN:.cpp=.o)
	${CXX} ${EXE_CXXFLAGS} -o $@ $^

%.d: %.cpp
	@set -e; rm -f $@; \
		$(CXX) -MM $(CXXFLAGS) $< > $@.$$$$; \
		sed 's,\($*\)\.o[ : ]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$

-include $(SRC:.cpp=.d)

clean :
	${RM} ${EXE} *.o *.d *.d.*
