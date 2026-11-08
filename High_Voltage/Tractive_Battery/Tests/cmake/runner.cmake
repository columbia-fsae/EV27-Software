# Generates the Unity runner (the file with main()) for one test file.
#
#   tb_generate_runner(<test .c file> <runner .c to write> [FAKES <module>...])
#
# Every function written as `void test_<name>(void)` starting at column 0 is a
# test. Before each one the runner calls hal_fakes_reset() and then
# fake_<module>_reset() for every FAKES entry, in order. Unity calls setUp()
# after that, so whatever setUp() configures is kept. If the test file has no
# setUp()/tearDown(), the runner defines empty ones.
#
# The test file is added to CMAKE_CONFIGURE_DEPENDS, so adding a test and
# running `cmake --build` regenerates the runner without re-running cmake.

function(tb_generate_runner test_src runner_out)
    cmake_parse_arguments(ARG "" "" "FAKES" ${ARGN})
    get_filename_component(test_file_name "${test_src}" NAME)

    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${test_src}")
    file(READ "${test_src}" content)
    # Make "start of a line" the same as "after a newline" for the regexes below
    set(content "\n${content}")

    set(test_regex "\nvoid[ \t]+(test_[A-Za-z0-9_]+)[ \t]*\\([ \t]*void[ \t]*\\)")
    string(REGEX MATCHALL "${test_regex}" matches "${content}")
    set(tests "")
    foreach(match IN LISTS matches)
        string(REGEX REPLACE "${test_regex}" "\\1" name "${match}")
        list(APPEND tests "${name}")
    endforeach()
    list(REMOVE_DUPLICATES tests)  # a prototype plus its definition count once

    if(NOT tests)
        message(FATAL_ERROR
            "${test_file_name}: no test functions found. Write each test as\n"
            "    void test_<name>(void) {\n"
            "starting at column 0.")
    endif()
    if(content MATCHES "\nint[ \t]+main[ \t]*\\(")
        message(FATAL_ERROR
            "${test_file_name} defines main(). Delete it: the runner is generated "
            "from the test_* functions (see Tests/cmake/runner.cmake).")
    endif()

    set(src "/* Generated from ${test_file_name} by Tests/cmake/runner.cmake. Do not edit. */\n")
    string(APPEND src "#include \"hal_fakes.h\"\n#include \"unity.h\"\n")
    foreach(fake IN LISTS ARG_FAKES)
        string(APPEND src "#include \"fake_${fake}.h\"\n")
    endforeach()
    string(APPEND src "\n")

    if(NOT content MATCHES "\nvoid[ \t]+setUp[ \t]*\\(")
        string(APPEND src "void setUp(void) {}\n")
    endif()
    if(NOT content MATCHES "\nvoid[ \t]+tearDown[ \t]*\\(")
        string(APPEND src "void tearDown(void) {}\n")
    endif()

    foreach(name IN LISTS tests)
        string(APPEND src "void ${name}(void);\n")
    endforeach()

    # Unity prints the file name passed to UnityBegin on every result line,
    # so pass the test file's name rather than this runner's.
    string(APPEND src "\nint main(void) {\n    UnityBegin(\"${test_file_name}\");\n")
    foreach(name IN LISTS tests)
        # Line of the test in the test file, for Unity's PASS/IGNORE lines
        # (content starts with an extra "\n", so the newlines before the
        # match equal the test's 1-based line number minus one)
        string(REGEX MATCH "\nvoid[ \t]+${name}[ \t]*\\(" decl "${content}")
        string(FIND "${content}" "${decl}" pos)
        string(SUBSTRING "${content}" 0 ${pos} before)
        string(REGEX MATCHALL "\n" newlines "${before}")
        list(LENGTH newlines line)
        math(EXPR line "${line} + 1")

        string(APPEND src "\n    hal_fakes_reset();\n")
        foreach(fake IN LISTS ARG_FAKES)
            string(APPEND src "    fake_${fake}_reset();\n")
        endforeach()
        string(APPEND src "    RUN_TEST(${name}, ${line});\n")
    endforeach()
    string(APPEND src "\n    return UNITY_END();\n}\n")

    # Only touches the file when the content changes, so unchanged runners
    # don't trigger a rebuild.
    file(GENERATE OUTPUT "${runner_out}" CONTENT "${src}")
endfunction()
