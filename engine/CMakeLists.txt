MACRO(DAS_TUTORIAL tutorial_name tutorial_src)
    add_executable(${tutorial_name} ${tutorial_src})
    TARGET_LINK_LIBRARIES(${tutorial_name} libDaScript Threads::Threads)
    ADD_DEPENDENCIES(${tutorial_name} libDaScript)
    SETUP_CPP11(${tutorial_name})
ENDMACRO()

####################
# tutorial 00 - nano
####################
SET(TUTORIAL_00_SRC
${CMAKE_SOURCE_DIR}/examples/tutorial/tutorial00.cpp
)
DAS_TUTORIAL(tutorial00 "${TUTORIAL_00_SRC}")