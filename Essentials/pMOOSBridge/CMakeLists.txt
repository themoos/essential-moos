#this builds the pMOOSBridge application
set(EXECNAME pMOOSBridge)

#what files are needed?
SET(SRCS MOOSBridgeMain.cpp MOOSCommunity.cpp MOOSBridge.cpp  MOOSCommunity.h MOOSBridge.h MOOSUDPLink.cpp MOOSUDPLink.h )

include_directories( ${${EXECNAME}_INCLUDE_DIRS} ${MOOSLib_INCLUDE_DIRS} ${MOOSLib_DEPEND_INCLUDE_DIRS})
add_executable(${EXECNAME} ${SRCS} )
target_link_libraries(${EXECNAME} ${MOOSLib_LIBRARIES} ${MOOSLib_DEPEND_LIBRARIES})


