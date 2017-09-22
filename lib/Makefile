TARGET = libcoeditor.a

all : ${TARGET}

include ../Makefile.inc

${TARGET} : $(SRC:.cpp=.o)
	ar r $@ $^