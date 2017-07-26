INC_DIR = ../include
CXXFLAGS = -std=c++11 \
		-g -I${INC_DIR} \
		-pedantic \
		-Wall \
		-Wextra \
		-Wno-sign-compare \
		-Wno-unused-parameter
RM = rm -f
