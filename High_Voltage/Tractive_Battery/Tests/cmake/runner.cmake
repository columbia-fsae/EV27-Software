
# tb_generate_runner(<test .c> <runner .c to write> [FAKES <modules>] [TESTS_OUT <var>])
#
# Writes the main() for one test file. It finds every `void test_<name>(void)`
# at column 0 and puts it in a table. The generated program:
#
#   test_<module>                   runs every test in this one process
#   test_<module> <name>            runs one test (with or without the test_ prefix)
#   test_<module> --list            prints the test names
#   test_<module> --opt-in <name>   runs <name> only if TB_TESTS_OPT_IN is set
#                                   (non-empty, not 0), otherwise exits 77 (skip)
#
# The fakes are reset before every test. TESTS_OUT receives the test function
# names, in file order, so add_fw_test can register one ctest test per function.
function(tb_generate_runner test_src runner_out)
    cmake_parse_arguments(ARG "" "TESTS_OUT" "FAKES" ${ARGN})
    get_filename_component(test_file_name "${test_src}" NAME)

    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${test_src}")
    file(READ "${test_src}" content)
    set(content "\n${content}")

    set(test_regex "\nvoid[ \t]+(test_[A-Za-z0-9_]+)[ \t]*\\([ \t]*void[ \t]*\\)")
    string(REGEX MATCHALL "${test_regex}" matches "${content}")
    set(tests "")
    foreach(match IN LISTS matches)
        string(REGEX REPLACE "${test_regex}" "\\1" name "${match}")
        list(APPEND tests "${name}")
    endforeach()
    list(REMOVE_DUPLICATES tests)

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
    string(APPEND src "#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n\n")
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

    string(APPEND src "
typedef struct {
    const char* name;
    UnityTestFunction func;
    int line;
} tb_test_t;

static const tb_test_t tb_tests[] = {
")
    foreach(name IN LISTS tests)
        string(REGEX MATCH "\nvoid[ \t]+${name}[ \t]*\\(" decl "${content}")
        string(FIND "${content}" "${decl}" pos)
        string(SUBSTRING "${content}" 0 ${pos} before)
        string(REGEX MATCHALL "\n" newlines "${before}")
        list(LENGTH newlines line)
        math(EXPR line "${line} + 1")
        string(APPEND src "    {\"${name}\", ${name}, ${line}},\n")
    endforeach()
    string(APPEND src "};
#define TB_TEST_COUNT (sizeof(tb_tests) / sizeof(tb_tests[0]))

static void tb_run(const tb_test_t* t) {
    hal_fakes_reset();
")
    foreach(fake IN LISTS ARG_FAKES)
        string(APPEND src "    fake_${fake}_reset();\n")
    endforeach()
    string(APPEND src "    UnityDefaultTestRun(t->func, t->name, t->line);
}

static const tb_test_t* tb_find(const char* arg) {
    for (size_t i = 0; i < TB_TEST_COUNT; i++) {
        const char* name = tb_tests[i].name;
        if (strcmp(arg, name) == 0 || strcmp(arg, name + strlen(\"test_\")) == 0) {
            return &tb_tests[i];
        }
    }
    return NULL;
}

static void tb_list(FILE* out) {
    for (size_t i = 0; i < TB_TEST_COUNT; i++) {
        fprintf(out, \"%s\\n\", tb_tests[i].name);
    }
}

static int tb_usage(const char* prog) {
    fprintf(stderr, \"usage: %s [--list | [--opt-in] <test name>]\\n\", prog);
    return 2;
}

int main(int argc, char** argv) {
    int opt_in = 0;
    int arg = 1;
    if (argc == 2 && strcmp(argv[1], \"--list\") == 0) {
        tb_list(stdout);
        return 0;
    }
    if (arg < argc && strcmp(argv[arg], \"--opt-in\") == 0) {
        opt_in = 1;
        arg++;
        if (arg >= argc) {
            return tb_usage(argv[0]);
        }
    }
    if (argc > arg + 1 || (arg < argc && argv[arg][0] == '-')) {
        return tb_usage(argv[0]);
    }

    const tb_test_t* only = NULL;
    if (arg < argc) {
        only = tb_find(argv[arg]);
        if (only == NULL) {
            fprintf(stderr, \"${test_file_name}: no test named '%s'. Tests in this file:\\n\", argv[arg]);
            tb_list(stderr);
            return 2;
        }
        if (opt_in) {
            const char* env = getenv(\"TB_TESTS_OPT_IN\");
            if (env == NULL || env[0] == '\\0' || strcmp(env, \"0\") == 0) {
                printf(\"%s is opt-in; skipped. Set TB_TESTS_OPT_IN=1 to run it.\\n\", only->name);
                return 77;
            }
        }
    }

    UnityBegin(\"${test_file_name}\");
    if (only != NULL) {
        tb_run(only);
    } else {
        for (size_t i = 0; i < TB_TEST_COUNT; i++) {
            tb_run(&tb_tests[i]);
        }
    }
    return UNITY_END();
}
")

    file(GENERATE OUTPUT "${runner_out}" CONTENT "${src}")
    if(ARG_TESTS_OUT)
        set(${ARG_TESTS_OUT} "${tests}" PARENT_SCOPE)
    endif()
endfunction()
