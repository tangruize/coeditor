include ../Makefile.inc

LIBS = libfrontend.so

SRC = $(wildcard *.cpp)

FRONT_END_CLI_LIB = libfrontend.so.1.0.1

FRONT_END = ${FRONT_END_CLI_LIB}

FRONT_END_CXXFLAGS := ${CXXFLAGS} -shared \
				 -Wl,-soname,libfrontend.so.1 \
				 -Wl,-efront_end_main \
				 -Wl,--version-script,front_end_version.map

CXXFLAGS += -fPIC

all : ${LIBS}

${FRONT_END_CLI_LIB} : front_end.o front_end_cli.o
	${CXX} ${FRONT_END_CXXFLAGS} -o $@ $^

libfrontend.so.1 : ${FRONT_END}
	ln -sf $< $@

libfrontend.so : libfrontend.so.1
	ln -sf $< $@

%.d: %.cpp
	@set -e; rm -f $@; \
		$(CXX) -MM $(CXXFLAGS) $< > $@.$$$$; \
		sed 's,\($*\)\.o[ : ]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$

-include $(SRC:.cpp=.d)

clean :
	${RM} ${LIBS} *.o *.so.* *.d *.d.*
