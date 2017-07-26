include ../Makefile.inc

LIBS = libfrontend.so

SRC = $(wildcard *.cpp)

FRONT_END_CLI_LIB = libfrontend.so.1.0.1

FRONT_END_CURSES_LIB = libfrontend.so.1.1.1

FRONT_END = ${FRONT_END_CURSES_LIB}

FRONT_END_CXXFLAGS := ${CXXFLAGS} -shared \
				 -Wl,-soname,libfrontend.so.1 \
				 -Wl,-efront_end_main \
				 -Wl,--version-script,front_end_version.map

CXXFLAGS += -fPIC

all : ${LIBS}

# Because of C++ name mangling, and our shared libraries use external code,
# dynamic linker probably cannot find the symbol if using another version,
# to avoid this problem, we must remove ELF file and re-link.
# Yes, it is due to the poor design (shared libraries using external code).
use-cli : ${FRONT_END_CLI_LIB}
	@ if [ `readlink libfrontend.so.1` != "$<" ]; then \
		echo using $<; \
		ln -sf $< libfrontend.so.1; \
		rm -f ../coeditor 2> /dev/null; \
	else \
		echo no change; \
	fi

use-curses : ${FRONT_END_CURSES_LIB}
	@ if [ `readlink libfrontend.so.1` != "$<" ]; then \
		echo using $<; \
		ln -sf $< libfrontend.so.1; \
		rm -f ../coeditor 2> /dev/null; \
	else \
		echo no change; \
	fi

${FRONT_END_CLI_LIB} : front_end.o front_end_cli.o
	${CXX} ${FRONT_END_CXXFLAGS} -o $@ $^

${FRONT_END_CURSES_LIB} : front_end.o front_end_curses.o
	${CXX} ${FRONT_END_CXXFLAGS} -o $@ $^ -lncurses

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
