include ../Makefile.inc

SRC = $(wildcard *.cpp)

SRC_NO_MAIN = $(shell find . -type f -name '*.cpp' ! -name main.cpp)

EXE = ../coeditor

EXE_STATIC = ../coeditor-static

LIB_DIR = ../lib

LIBS = $(wildcard ${LIB_DIR}/*.so)

LIBS_STATIC = $(wildcard ${LIB_DIR}/*.a)

all : ${EXE}

static : ${EXE_STATIC}

${EXE} : main.o ${LIBS} $(SRC_NO_MAIN:.cpp=.o)
	${CXX} ${CXXFLAGS} -Wl,-rpath,'$$ORIGIN'/lib -o $@ $^

${EXE_STATIC} : main.o ${LIBS_STATIC} $(SRC_NO_MAIN:.cpp=.o)
	${CXX} ${CXXFLAGS} -o $@ $^ -lncurses

%.d: %.cpp
	@set -e; rm -f $@; \
		$(CXX) -MM $(CXXFLAGS) $< > $@.$$$$; \
		sed 's,\($*\)\.o[ : ]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$

-include $(SRC:.cpp=.d)

clean :
	${RM} ${EXE} ${EXE_STATIC} *.o *.d *.d.*
