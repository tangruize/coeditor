include ../Makefile.inc

SRC = $(wildcard *.cpp)

EXE = ../coeditor

LIB_DIR = ../lib

LIBS = $(wildcard ${LIB_DIR}/*.so)

EXE_CXXFLAGS := ${CXXFLAGS} -Wl,-rpath,'$$ORIGIN'/lib

all : ${EXE}

${EXE} : $(SRC:.cpp=.o) ${LIBS}
	${CXX} ${EXE_CXXFLAGS} -o $@ $^

%.d: %.cpp
	@set -e; rm -f $@; \
		$(CXX) -MM $(CXXFLAGS) $< > $@.$$$$; \
		sed 's,\($*\)\.o[ : ]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$

-include $(SRC:.cpp=.d)

clean :
	${RM} ${EXE} *.o *.d *.d.*
