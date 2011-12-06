#this builds the pScheduler application
set(EXECNAME pScheduler)

#what files are needed?
SET(SRCS   MOOSScheduler.cpp  pSchedulerMain.cpp)

include_directories( ${${EXECNAME}_INCLUDE_DIRS} ${MOOSLib_INCLUDE_DIRS} ${MOOSLib_DEPEND_INCLUDE_DIRS})
add_executable(${EXECNAME} ${SRCS} )
target_link_libraries(${EXECNAME} ${MOOSLib_LIBRARIES} ${MOOSLib_DEPEND_LIBRARIES})

