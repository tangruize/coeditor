# DIRS is a list of all subdirectories containing makefiles
# (The library directory is first so that the library gets built first)
#

DIRS = lib src

BUILD_DIRS = ${DIRS}

#Dummy targets for building and clobbering everything in all subdirectories

all:
	@ for dir in ${BUILD_DIRS}; do (cd $${dir}; ${MAKE}) ; done

clean:
	@ for dir in ${BUILD_DIRS}; do (cd $${dir}; ${MAKE} clean) ; done

pre-use-cli:
	@ cd lib; ${MAKE} use-cli

pre-use-curses:
	@ cd lib; ${MAKE} use-curses

use-cli: pre-use-cli all

use-curses: pre-use-curses all

