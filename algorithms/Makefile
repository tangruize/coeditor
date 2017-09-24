CXXFLAGS := -fPIC

SRC = $(wildcard *.cpp)
DLL = $(addprefix ../lib/lib, $(SRC:.cpp=.so))
DEFAULT_DLL = libcscw.so
SONAME = libalgo.so

all: ${DLL}
	@if [ ! -f ../lib/${SONAME} ]; then ln -s ${DEFAULT_DLL} ../lib/${SONAME}; fi

include ../Makefile.inc

../lib/lib%.so: %.o ${LIBS}
	${CXX} ${CXXFLAGS} -shared -Wl,-soname,${SONAME} -o $@ $^ 