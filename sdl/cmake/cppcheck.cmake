
file(GLOB_RECURSE ALL_SOURCE_FILES *.c *.cpp *.h)

foreach (SOURCE_FILE ${ALL_SOURCE_FILES})
 string(FIND ${SOURCE_FILE} CMakeFiles PROJECT_TRDPARTY_DIR_FOUND)
 if (NOT ${PROJECT_TRDPARTY_DIR_FOUND} EQUAL -1)
         list(REMOVE_ITEM ALL_SOURCE_FILES ${SOURCE_FILE})
 endif ()
endforeach ()

add_custom_target(
    cppcheck
    COMMAND /usr/bin/cppcheck
    --enable=warning,performance,portability,information,missingInclude
#    --enable=all
    --suppress=missingIncludeSystem
    --library=std.cfg
    --library=sdl.cfg
    --library=gnu.cfg
    --library=posix.cfg
    --std=c11
    --template="[{severity}][{id}] {message} {callstack} \(On {file}:{line}\)"
    --verbose
    --quiet
#    --check-config
#    -I /usr/include/x86_64-linux-gnu/
#    -I /home/marco/aic/src/player/sdl/include/
#    -I /usr/include/SDL/
    ${ALL_SOURCE_FILES}
)
