# DIRS is a list of all subdirectories containing makefiles
# (The library directory is first so that the library gets built first)
#

DIRS = lib server src

BUILD_DIRS = ${DIRS}

# Dummy targets for building and clobbering everything in all subdirectories

all:
	@ for dir in ${BUILD_DIRS}; do (cd $${dir}; ${MAKE}) ; done

static:
	@ for dir in ${BUILD_DIRS}; do (cd $${dir}; ${MAKE} static) ; done

clean:
	@ for dir in ${BUILD_DIRS}; do (cd $${dir}; ${MAKE} clean) ; done

pre-use-cli:
	@ cd lib; ${MAKE} cli

pre-use-curses:
	@ cd lib; ${MAKE} curses

cli: pre-use-cli all

curses: pre-use-curses all

