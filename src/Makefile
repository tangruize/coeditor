include ../Makefile.inc

SRC = $(wildcard *.cpp)

SRC_NO_MAIN = $(shell find . -type f -name '*.cpp' ! -name main.cpp ! -name init.cpp)

EXE = ../coeditor

#EXE_STATIC = ../coeditor-static

LIB_DIR = ../lib

LIBS = $(wildcard ${LIB_DIR}/*.so)

LIBS_STATIC = $(wildcard ${LIB_DIR}/*.a)

all : ${EXE}

#static : ${EXE_STATIC}

${EXE} : main.o init.o ${LIBS} $(SRC_NO_MAIN:.cpp=.o)
#${EXE} : $(SRC:.cpp=.o) ${LIBS}
	${CXX} ${CXXFLAGS} -Wl,-rpath,'$$ORIGIN'/lib -o $@ $^ -pthread

#${EXE_STATIC} : main.o ${LIBS_STATIC} $(SRC_NO_MAIN:.cpp=.o)
#${EXE_STATIC} : $(SRC:.cpp=.o) ${LIBS_STATIC}
#	${CXX} -o $@ $^ ${CXXFLAGS} -static -pthread -lncurses -ltinfo -lgpm -Wl,--whole-archive -lpthread -Wl,--no-whole-archive

%.d: %.cpp
	@set -e; rm -f $@; \
		$(CXX) -MM $(CXXFLAGS) $< > $@.$$$$; \
		sed 's,\($*\)\.o[ : ]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$

-include $(SRC:.cpp=.d)

clean :
	${RM} ${EXE} ${EXE_STATIC} *.o *.d *.d.*
