FIND_LIBRARY(KABC_LIBRARY NAMES kabc)
FIND_PATH(KABC_INCLUDE_DIR kabc_export.h /usr/include/kabc /usr/local/include/kabc $ENV{KDEDIR}/include/kabc )


IF (KABC_INCLUDE_DIR AND KABC_LIBRARY)
	SET(KABC_FOUND TRUE)
ELSE (KABC_INCLUDE_DIR AND KABC_LIBRARY)
	SET(KABC_FOUND FALSE)
ENDIF (KABC_INCLUDE_DIR AND KABC_LIBRARY)


IF (KABC_FOUND)
	IF (NOT Kabc_FIND_QUIETLY)
		MESSAGE(STATUS "Found Kabc library : ${KABC_LIBRARY}")
	ENDIF (NOT Kabc_FIND_QUIETLY)
ELSE (KABC_FOUND)
	IF (Kabc_FIND_REQUIRED)
		MESSAGE(FATAL_ERROR "Could not find Kabc : You might install kdepimlibs5-dev.\n      sudo apt-get install kdepimlibs5-dev\n")
	ENDIF (Kabc_FIND_REQUIRED)
ENDIF (KABC_FOUND)

