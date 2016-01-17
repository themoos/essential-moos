#this builds the pLogger application
set(EXECNAME pAntler)

find_package(MOOS 10)

#what files are needed?
SET(SRCS  AntlerMain.cpp Antler.cpp)

IF (WIN32)
    SET(SRCS ${SRCS} XPCProcess.cpp XPCProcessAttrib.cpp)
ENDIF(WIN32)

include_directories( ${${EXECNAME}_INCLUDE_DIRS} ${MOOS_INCLUDE_DIRS} ${MOOS_DEPEND_INCLUDE_DIRS})
add_executable(${EXECNAME} ${SRCS} )
target_link_libraries(${EXECNAME} ${MOOS_LIBRARIES} ${MOOS_DEPEND_LIBRARIES})

INSTALL(TARGETS ${EXECNAME}
  RUNTIME DESTINATION bin
)
