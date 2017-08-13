include ../Makefile.inc

SRC = $(wildcard *.cpp)

EXE = ../jupiter-ot

LIB_DIR = ../lib

LIBS = ${LIB_DIR}/libxform.a

all : ${EXE}

${EXE} : $(SRC:.cpp=.o)
	${CXX} ${CXXFLAGS} -o $@ $^ ${LIBS}

%.d: %.cpp
	@set -e; rm -f $@; \
		$(CXX) -MM $(CXXFLAGS) $< > $@.$$$$; \
		sed 's,\($*\)\.o[ : ]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$

-include $(SRC:.cpp=.d)

clean :
	${RM} ${EXE} ${EXE_STATIC} *.o *.d *.d.*
