# Windows x64 default thread stack

The MinGW x64 toolchain reserves 2 MiB of virtual address space per default-sized
thread, retaining an 8 KiB initial commit. Previously it reserved only 128 KiB,
one-sixteenth of GNU ld's normal PE default. The legacy x86 toolchain is unchanged.

Windows uses the executable's stack policy for threads created with a zero stack
size. This can affect library and graphics-driver workers as well as application
code. Reserve is address space, not an immediate allocation of 2 MiB physical RAM
for every thread.

Related upstream reports [#384](https://github.com/shinyquagsire23/OpenJKDF2/issues/384)
and [#391](https://github.com/shinyquagsire23/OpenJKDF2/issues/391) contain stack
overflows inside AMD's `atio6axx.dll`. A larger default stack addresses a concrete
build limitation. It is not proof that those drivers use default-sized threads,
nor that every AMD startup crash has this cause.

## Reproduce the allocation regression

Build with the actual production toolchain on a host with CMake and x64 MinGW:

```sh
python3 -B src/Tests/run_windows_stack_test.py --build-dir /tmp/openjkdf2-stack-probe
```

Use a fresh external build directory. On Windows, observe the built probe's
actual main and worker stacks without deliberately overflowing them:

```powershell
python -B src/Tests/run_windows_stack_test.py --check-exe C:\explicit\probe\windows_stack_probe.exe --run-native
```

Use `--run-native` only for the probe. The following check reads a game executable
without launching it, and is also used by the fork's Windows packaging workflow:

```sh
python3 -B src/Tests/run_windows_stack_test.py --check-exe /explicit/path/openjkdf2-64.exe
```

The regression rejects less than the documented Windows default of 1 MiB and
invalid commit/reserve relationships. The independent native control requests
exactly 1 MiB regardless of the executable policy.

On November 1, 2026, the original policy produced a 128 KiB default worker and
a 256 KiB initial-thread allocation on the test machine. The corrected build
produced 2 MiB for both. The explicit control stayed at 1 MiB in both cases.
Restoring the original policy in a separate build reproduced the failure.
These are allocation tests, not AMD-hardware or gameplay acceptance.

## Existing build directories

The toolchain populates `CMAKE_EXE_LINKER_FLAGS_INIT`. CMake may retain old flags
in an existing cache. Prefer a fresh build directory; when deliberately reusing
one, inspect its custom flags before clearing the relevant cache entries and
reconfiguring. Always run the header check on the final packaged executable.

## Affected-hardware acceptance

An RX 7900 XT / Windows 11 / driver 26.10.2 failure has been reported without a
crash trace. Matching its `.RPT` exception and testing this exact candidate on
that hardware remain necessary before calling that incident fixed. Record the
executable SHA-256, actual driver DLL version, last startup log lines, and whether
menus and a level load successfully. No driver downgrade is required to test this
candidate; renderer/context behavior is unchanged.

References: [Microsoft thread stack size](https://learn.microsoft.com/en-us/windows/win32/procthread/thread-stack-size),
[CreateThread](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createthread),
[GNU ld --stack](https://sourceware.org/binutils/docs/ld/Options.html#index-_002d_002dstack).
