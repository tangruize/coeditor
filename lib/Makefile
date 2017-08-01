include ../Makefile.inc

LIBS = libfrontend.so.1 libinetsockets.so

SRC = $(wildcard *.cpp)

FRONT_END_CLI_LIB = libfrontend.so.1.0.1

FRONT_END_CURSES_LIB = libfrontend.so.1.1.1

FRONT_END_STATIC_DEFAULT = libfrontend.a.1.1.1

FRONT_END_DEFAULT = ${FRONT_END_CURSES_LIB}

FRONT_END_CXXFLAGS := ${CXXFLAGS} -shared \
				 -Wl,-soname,libfrontend.so.1 \
				 -Wl,-efrontEndMain

CXXFLAGS += -fPIC

all : ${LIBS}

static : ${LIBS}

cli : ${FRONT_END_CLI_LIB}
	@ libname=`readlink libfrontend.so.1`; \
		if [ $${libname:-"NULL"} != "$<" ]; then \
		echo using $<; \
		ln -sf $< libfrontend.so.1; \
		ln -sf libfrontend.a.1.0.1 libfrontend.a; \
		rm -f ../coeditor 2> /dev/null; \
	else \
		echo no change; \
	fi

curses : ${FRONT_END_CURSES_LIB}
	@ libname=`readlink libfrontend.so.1`; \
		if [ $${libname:-"NULL"} != "$<" ]; then \
		echo using $<; \
		ln -sf $< libfrontend.so.1; \
		ln -sf libfrontend.a.1.1.1 libfrontend.a; \
	else \
		echo no change; \
	fi

${FRONT_END_CLI_LIB} : front_end_v1.o front_end_cli.o
	${CXX} ${FRONT_END_CXXFLAGS} -Wl,--version-script,front_end_version1.map -o $@ $^
	ar r libfrontend.a.1.0.1 $^

${FRONT_END_CURSES_LIB} : front_end_v2.o front_end_cli.o front_end_curses.o
	${CXX} ${FRONT_END_CXXFLAGS} -Wl,--version-script,front_end_version2.map -o $@ $^ -lncurses -pthread
	ar r libfrontend.a.1.1.1 $^

libfrontend.so.1 : ${FRONT_END_CLI_LIB} ${FRONT_END_CURSES_LIB}
	-ln -s ${FRONT_END_DEFAULT} $@ 2> /dev/null
	-ln -s ${FRONT_END_STATIC_DEFAULT} libfrontend.a 2> /dev/null
	-ln -s libfrontend.so.1 libfrontend.so 2> /dev/null
	touch -c $@ libfrontend.a

libinetsockets.so : inet_sockets.o
	${CXX} ${CXXFLAGS} -shared -o $@ $^
	ar r libinetsockets.a $^

%.d: %.cpp
	@set -e; rm -f $@; \
		$(CXX) -MM $(CXXFLAGS) $< > $@.$$$$; \
		sed 's,\($*\)\.o[ : ]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$

-include $(SRC:.cpp=.d)

clean :
	${RM} ${LIBS} *.o *.so *.so.* *.d *.d.* *.a *.a.*
