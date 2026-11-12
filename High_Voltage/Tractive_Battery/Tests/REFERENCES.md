# References

Start with the first four. Come back to the rest when a topic comes up.

## Getting started

- [Memfault: Unit testing basics](https://interrupt.memfault.com/blog/unit-testing-basics):
  the best short intro to unit testing embedded C, with a CppUTest example.
- [Memfault: Mocking and fakes](https://interrupt.memfault.com/blog/unit-test-mocking):
  when to stub, fake or mock, and link seams in practice.
- [FFF repository and README](https://github.com/meekrosoft/fff): the main
  FFF documentation. Every feature used here, plus custom-fake details.
  See also its [driver testing example](https://github.com/meekrosoft/fff/tree/master/examples/driver_testing).
- [Unity: getting started](https://www.throwtheswitch.org/getting-started-with-unity):
  the project's own introduction to writing and running tests.

## Unity documentation (v2.7.0, the version in `vendor/`)

- [Docs folder](https://github.com/ThrowTheSwitch/Unity/tree/v2.7.0/docs): all
  of the guides below.
- [UnityAssertionsReference.md](https://github.com/ThrowTheSwitch/Unity/blob/v2.7.0/docs/UnityAssertionsReference.md):
  every assertion, including arrays, bit masks, floats and `_MESSAGE` variants.
- [UnityConfigurationGuide.md](https://github.com/ThrowTheSwitch/Unity/blob/v2.7.0/docs/UnityConfigurationGuide.md):
  options like `UNITY_INCLUDE_DOUBLE` and `UNITY_SUPPORT_64` (both set in our
  CMake), and float precision.
- [UnityGettingStartedGuide.md](https://github.com/ThrowTheSwitch/Unity/blob/v2.7.0/docs/UnityGettingStartedGuide.md):
  `setUp`/`tearDown`, `RUN_TEST` and what the output means.
- [UnityHelperScriptsGuide.md](https://github.com/ThrowTheSwitch/Unity/blob/v2.7.0/docs/UnityHelperScriptsGuide.md):
  Unity's own runner generator (ours is the CMake version of the same idea)
  and `TEST_CASE` for running one test over a table of inputs.

## How other projects set it up

- [Unity + CMake integration](https://embedded-focus.com/blog/cmake-unity-integration/):
  a walkthrough of a CMake build for Unity tests, close to ours.
- [Purdue Electric Racing's tests](https://github.com/PurdueElectricRacing/monorepo/tree/master/tests):
  another FSAE team's firmware tests, using GoogleTest, with CI that runs
  sanitizers and coverage.
- [CMock](https://github.com/ThrowTheSwitch/CMock) and
  [Ceedling](https://github.com/ThrowTheSwitch/Ceedling): the generated-mock
  alternative to FFF, and a build system around Unity.

## Designing good tests

| Topic | Read |
|---|---|
| Stubs, spies, mocks and fakes | [TestDouble](https://martinfowler.com/bliki/TestDouble.html), [Mocks Aren't Stubs](https://martinfowler.com/articles/mocksArentStubs.html) |
| Test behaviour, not implementation | [Software Engineering at Google, ch. 12](https://abseil.io/resources/swe-book/html/ch12.html) |
| Don't overuse fakes: link the real `battery_model.c` when you can | [SWE at Google, ch. 13](https://abseil.io/resources/swe-book/html/ch13.html) |
| Structure and naming | [ArrangeActAssert](http://wiki.c2.com/?ArrangeActAssert), [GivenWhenThen](https://martinfowler.com/bliki/GivenWhenThen.html), [naming](https://osherove.com/blog/2005/4/3/naming-standards-for-unit-tests.html) |
| Readable over DRY | [Tests too DRY](https://testing.googleblog.com/2019/12/testing-on-toilet-tests-too-dry-make.html) |
| Fixtures and isolation | [Fresh Fixture](http://xunitpatterns.com/Fresh%20Fixture.html), [xUnit Test Patterns](http://xunitpatterns.com/) |
| Choosing inputs: classes and edges | [Equivalence partitioning](https://en.wikipedia.org/wiki/Equivalence_partitioning), [boundary values](https://en.wikipedia.org/wiki/Boundary-value_analysis) |
| State machines (`bsm.c`) | [State transition table](https://en.wikipedia.org/wiki/State_transition_table) |
| A test for every bug, written before the fix | [Regression testing](https://en.wikipedia.org/wiki/Regression_testing), [TDD](https://martinfowler.com/bliki/TestDrivenDevelopment.html) |
| Pinning today's output before a refactor (`KalmanSOC_Update`) | [Characterization test](https://en.wikipedia.org/wiki/Characterization_test) |
| What unit tests can't cover | [Test pyramid](https://martinfowler.com/articles/practical-test-pyramid.html), [HIL](https://en.wikipedia.org/wiki/Hardware-in-the-loop_simulation) |
| Flaky tests | [Flaky tests at Google](https://testing.googleblog.com/2016/05/flaky-tests-at-google-and-how-we.html) |

## C and embedded pitfalls worth a test

| Topic | Read |
|---|---|
| Unsigned wraparound (`HAL_GetTick()` timeouts) | [CERT INT30-C](https://wiki.sei.cmu.edu/confluence/display/c/INT30-C.+Ensure+that+unsigned+integer+operations+do+not+wrap) |
| Float to integer (`clamp_u8` with NaN) | [CERT FLP34-C](https://wiki.sei.cmu.edu/confluence/display/c/FLP34-C.+Ensure+that+floating-point+conversions+are+within+range+of+the+new+type) |
| Comparing floats (`TEST_ASSERT_FLOAT_WITHIN` tolerances) | [Comparing floats](https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/) |
| Host vs. target sizes | [Arithmetic types](https://en.cppreference.com/w/c/language/arithmetic_types), [stdint.h](https://en.cppreference.com/w/c/types/integer) |
| Byte order in CAN payloads | [Endianness](https://en.wikipedia.org/wiki/Endianness) |
| Out-of-bounds and uninitialised reads | [CERT ARR30-C](https://wiki.sei.cmu.edu/confluence/display/c/ARR30-C.+Do+not+form+or+use+out-of-bounds+pointers+or+array+subscripts), [CERT EXP33-C](https://wiki.sei.cmu.edu/confluence/display/c/EXP33-C.+Do+not+read+uninitialized+memory) |
| Interrupts and shared data | [volatile](https://en.cppreference.com/w/c/language/volatile), [Cortex-M exceptions](https://interrupt.memfault.com/blog/arm-cortex-m-exceptions-and-nvic) |

## Tools

| Topic | Read |
|---|---|
| CTest filters, repeats, timeouts | [ctest](https://cmake.org/cmake/help/latest/manual/ctest.1.html) |
| Sanitizers | [ASan](https://clang.llvm.org/docs/AddressSanitizer.html), [UBSan](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html), [GCC options](https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html) |
| Coverage | [gcov](https://gcc.gnu.org/onlinedocs/gcc/Gcov.html), [gcovr](https://gcovr.com/en/stable/) |
| Seeing what an FFF macro expands to | [`gcc -E`](https://gcc.gnu.org/onlinedocs/gcc/Preprocessor-Options.html), [variadic macros](https://gcc.gnu.org/onlinedocs/cpp/Variadic-Macros.html) |
| Weak HAL callbacks (`HAL_ADC_ConvCpltCallback`) | [GCC function attributes](https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html) |
| Link seams | [Feathers, "Seams"](https://www.informit.com/articles/article.aspx?p=359417&seqNum=3) |

## Books

- James Grenning, *[Test-Driven Development for Embedded C](https://pragprog.com/titles/jgade/test-driven-development-for-embedded-c/)*:
  the standard book on testing C firmware on a PC.
- *[Software Engineering at Google](https://abseil.io/resources/swe-book/html/ch11.html)*,
  chapters 11–13 (free online).
