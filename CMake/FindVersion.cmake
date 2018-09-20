# Function for determining the OTTD version string
# Takes no arguments, sets the following variables into the parent scope:
# (effectively, it returns these values) :
# CFG_VERSION  - final version string
# CFG_ISODATE  - date of last commit/modification
# CFG_MODIFIED - modification state of the tree (0 - not modified, 1 - unknown, 2 - modified)
# CFG_HASH     - commit hash of the last commit
function (FindVersion)
    # defaults
    set(CFG_VERSION "" PARENT_SCOPE)
    set(CFG_ISODATE "" PARENT_SCOPE)
    set(CFG_MODIFIED "1" PARENT_SCOPE)
    set(CFG_HASH, "" PARENT_SCOPE)

    find_package(Git)
    if (NOT (GIT_FOUND AND IS_DIRECTORY "${CMAKE_SOURCE_DIR}/.git"))
        return ()
    endif ()

    # Make sure LC_ALL is set to something desirable
    set(SAVED_LC_ALL "$ENV{LC_ALL}")
    set(ENV{LC_ALL} C)

    # Refresh the index to make sure file stat info is in sync, then look for modifications
    execute_process(COMMAND ${GIT_EXECUTABLE} update-index --refresh
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                    OUTPUT_QUIET
    )

    # See if git tree is modified
    execute_process(COMMAND ${GIT_EXECUTABLE} diff-index HEAD
                    OUTPUT_VARIABLE IS_MODIFIED
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    if (NOT IS_MODIFIED STREQUAL "")
        set(MODIFIED "2")
    else ()
        set(MODIFIED "0")
    endif ()
    set(CFG_MODIFIED "${MODIFIED}" PARENT_SCOPE)

    # Get last commit hash
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --verify HEAD
                    OUTPUT_VARIABLE FULLHASH
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                    ERROR_QUIET
    )
    set(CFG_HASH "${FULLHASH}" PARENT_SCOPE)

    string(SUBSTRING "${FULLHASH}" 0 8 SHORTHASH)

    # Get the last commit date
    execute_process(COMMAND ${GIT_EXECUTABLE} show -s --pretty=format:%ci HEAD
                    OUTPUT_VARIABLE COMMITDATE
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    string(REGEX REPLACE "([0-9]+)-([0-9]+)-([0-9]+).*" "\\1\\2\\3" COMMITDATE "${COMMITDATE}")
    set(CFG_ISODATE "${COMMITDATE}" PARENT_SCOPE)

    # Get the branch
    execute_process(COMMAND ${GIT_EXECUTABLE} symbolic-ref -q HEAD
                    OUTPUT_VARIABLE BRANCH
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                    ERROR_QUIET
    )
    string(REGEX REPLACE ".*/" "" BRANCH "${BRANCH}")
    string(REGEX REPLACE "^master$" "" BRANCH "${BRANCH}")

    # Get the tag
    execute_process(COMMAND ${GIT_EXECUTABLE} name-rev --name-only --tags --no-undefined HEAD
                    OUTPUT_VARIABLE TAG
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                    ERROR_QUIET
    )
    string(REGEX REPLACE "\^0$" "" TAG "${TAG}")

    # Set the version string
    if (NOT TAG STREQUAL "")
        set(VERSION "${TAG}")
    elseif (BRANCH STREQUAL "master")
        set(VERSION "${COMMITDATE}-g${SHORTHASH}")
    else ()
        set(VERSION "${COMMITDATE}-${BRANCH}-g${SHORTHASH}")
    endif ()

    if (MODIFIED STREQUAL "2")
        set(VERSION "${VERSION}M")
    endif ()

    set(CFG_VERSION "${VERSION}" PARENT_SCOPE)

    # Restore
    set(ENV{LC_ALL} "${SAVED_LC_ALL}")
endfunction (FindVersion)
