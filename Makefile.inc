INC_DIR = ../include
CXXFLAGS += -std=c++11 \
		-g -O2 -I${INC_DIR} \
		-pedantic \
		-Wall \
		-Wextra \
		-Wno-sign-compare \
		-Wno-unused-result \
		-Wno-unused-parameter -DNDEBUG

EXE ?= dummy

RM = rm -f

SRC = $(wildcard *.cpp)

LIB_DIR = ../lib

LIBS = $(wildcard ${LIB_DIR}/*.a)

${EXE} : $(SRC:.cpp=.o) ${LIBS}
	${CXX} ${CXXFLAGS} -o $@ $^ ${LDFLAGS}

%.d: %.cpp
	@set -e; rm -f $@; \
		$(CXX) -MM $(CXXFLAGS) $< > $@.$$$$; \
		sed 's,\($*\)\.o[ : ]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$

-include $(SRC:.cpp=.d)

clean :
	${RM} ${EXE} ${EXE_STATIC} *.o *.d *.d.* *.a *.so
