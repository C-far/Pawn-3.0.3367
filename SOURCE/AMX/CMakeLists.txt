#build file for CMake, see http://www.cmake.org/

PROJECT(pawnamx)

# check for optional include files
INCLUDE(${CMAKE_ROOT}/Modules/CheckIncludeFile.cmake)
CHECK_INCLUDE_FILE("unistd.h" HAVE_UNISTD_H)
IF(HAVE_UNISTD_H)
  ADD_DEFINITIONS(-DHAVE_UNISTD_H)
ENDIF(HAVE_UNISTD_H)
CHECK_INCLUDE_FILE("inttypes.h" HAVE_INTTYPES_H)
IF(HAVE_INTTYPES_H)
  ADD_DEFINITIONS(-DHAVE_INTTYPES_H)
ENDIF(HAVE_INTTYPES_H)
CHECK_INCLUDE_FILE("stdint.h" HAVE_STDINT_H)
IF(HAVE_STDINT_H)
  ADD_DEFINITIONS(-DHAVE_STDINT_H)
ENDIF(HAVE_STDINT_H)
CHECK_INCLUDE_FILE("alloca.h" HAVE_ALLOCA_H)
IF(HAVE_ALLOCA_H)
  ADD_DEFINITIONS(-DHAVE_ALLOCA_H)
ENDIF(HAVE_ALLOCA_H)

ADD_DEFINITIONS(-DFLOATPOINT -DFIXEDPOINT)
IF (UNIX)
  ADD_DEFINITIONS(-DLINUX -g)
  INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/../linux)
  LINK_LIBRARIES(m dl) #CMake adds the "lib" prefix, so "m" becomes "libm"
ENDIF (UNIX)
IF(WIN32)
  ADD_DEFINITIONS(-DAMXEXPORT=__stdcall -DAMX_NATIVE_CALL=__stdcall -DSTDECL)
ENDIF(WIN32)

# --------------------------------------------------------------------------
# Extension modules

# amxArgs
SET(ARGS_SRCS amxargs.c amx.c)
ADD_LIBRARY(amxArgs SHARED ${ARGS_SRCS})
IF(WIN32)
  SET(ARGS_SRCS ${ARGS_SRCS} dllmain.c amxargs.rc)
  SET_TARGET_PROPERTIES(amxArgs PROPERTIES LINK_FLAGS "/export:amx_ArgsInit /export:amx_ArgsCleanup /export:amx_ArgsSetCmdLine")
ENDIF(WIN32)
IF(UNIX)
  ADD_CUSTOM_COMMAND(TARGET amxArgs POST_BUILD COMMAND cp ARGS libamxArgs.so amxArgs.so)
  ADD_CUSTOM_COMMAND(TARGET amxArgs POST_BUILD COMMAND strip ARGS -K amx_ArgsInit -K amx_ArgsCleanup -K amx_ArgsSetCmdLine amxArgs.so)
ENDIF(UNIX)

# amxFile
SET(ARGS_FILE amxfile.c amx.c)
IF(WIN32)
  SET(ARGS_FILE ${ARGS_FILE} dllmain.c amxfile.rc)
  SET_TARGET_PROPERTIES(amxFile PROPERTIES LINK_FLAGS "/export:amx_FileInit /export:amx_FileCleanup")
ENDIF(WIN32)
ADD_LIBRARY(amxFile SHARED ${ARGS_FILE})
IF(UNIX)
  ADD_CUSTOM_COMMAND(TARGET amxFile POST_BUILD COMMAND cp ARGS libamxFile.so amxFile.so)
  ADD_CUSTOM_COMMAND(TARGET amxFile POST_BUILD COMMAND strip ARGS -K amx_FileInit -K amx_FileCleanup amxFile.so)
ENDIF(UNIX)

# amxFixed
SET(ARGS_FIXED fixed.c amx.c)
IF(WIN32)
  SET(ARGS_FIXED ${ARGS_FIXED} dllmain.c amxfixed.rc)
  SET_TARGET_PROPERTIES(amxFixed PROPERTIES LINK_FLAGS "/export:amx_FixedInit /export:amx_FixedCleanup")
ENDIF(WIN32)
ADD_LIBRARY(amxFixed SHARED ${ARGS_FIXED})
IF(UNIX)
  ADD_CUSTOM_COMMAND(TARGET amxFixed POST_BUILD COMMAND cp ARGS libamxFixed.so amxFixed.so)
  ADD_CUSTOM_COMMAND(TARGET amxFixed POST_BUILD COMMAND strip ARGS -K amx_FixedInit -K amx_FixedCleanup amxFixed.so)
ENDIF(UNIX)

# amxFloat
SET(ARGS_FLOAT float.c amx.c)
IF(WIN32)
  SET(ARGS_FLOAT ${ARGS_FLOAT} dllmain.c amxfloat.rc)
  SET_TARGET_PROPERTIES(amxFloat PROPERTIES LINK_FLAGS "/export:amx_FloatInit /export:amx_FloatCleanup")
ENDIF(WIN32)
ADD_LIBRARY(amxFloat SHARED ${ARGS_FLOAT})
IF(UNIX)
  ADD_CUSTOM_COMMAND(TARGET amxFloat POST_BUILD COMMAND cp ARGS libamxFloat.so amxFloat.so)
  ADD_CUSTOM_COMMAND(TARGET amxFloat POST_BUILD COMMAND strip ARGS -K amx_FloatInit -K amx_FloatInit amxFloat.so)
ENDIF(UNIX)

# amxString
SET(ARGS_STRING amxstring.c amx.c)
IF(WIN32)
  SET(ARGS_STRING ${ARGS_STRING} dllmain.c amxstring.rc)
  SET_TARGET_PROPERTIES(amxString PROPERTIES LINK_FLAGS "/export:amx_StringInit /export:amx_StringCleanup")
ENDIF(WIN32)
ADD_LIBRARY(amxString SHARED ${ARGS_STRING})
IF(UNIX)
  ADD_CUSTOM_COMMAND(TARGET amxString POST_BUILD COMMAND cp ARGS libamxString.so amxString.so)
  ADD_CUSTOM_COMMAND(TARGET amxString POST_BUILD COMMAND strip ARGS -K amx_StringInit -K amx_StringCleanup amxString.so)
ENDIF(UNIX)

# amxTime
SET(ARGS_TIME amxtime.c amx.c)
IF(WIN32)
  SET(ARGS_TIME ${ARGS_TIME} dllmain.c amxtime.rc)
  SET_TARGET_PROPERTIES(amxTime PROPERTIES LINK_FLAGS "/export:amx_TimeInit /export:amx_TimeCleanup")
ENDIF(WIN32)
ADD_LIBRARY(amxTime SHARED ${ARGS_TIME})
IF(UNIX)
  ADD_CUSTOM_COMMAND(TARGET amxTime POST_BUILD COMMAND cp ARGS libamxTime.so amxTime.so)
  ADD_CUSTOM_COMMAND(TARGET amxTime POST_BUILD COMMAND strip ARGS -K amx_TimeInit -K amx_TimeCleanup amxTime.so)
ENDIF(UNIX)

# --------------------------------------------------------------------------
# Simple run-time (example program)

SET(PAWNRUN_SRCS pawnrun.c amx.c amxcore.c amxcons.c)
IF (UNIX)
  SET(PAWNRUN_SRCS ${PAWNRUN_SRCS} ../linux/getch.c)
ENDIF (UNIX)
ADD_EXECUTABLE(pawnrun ${PAWNRUN_SRCS})

# --------------------------------------------------------------------------
# Simple console debugger

SET(PAWNDBG_SRCS pawndbg.c amx.c amxcore.c amxcons.c amxdbg.c)
IF (UNIX)
  SET(PAWNDBG_SRCS ${PAWNDBG_SRCS} ../linux/getch.c)
ENDIF (UNIX)
ADD_EXECUTABLE(pawndbg ${PAWNDBG_SRCS})
