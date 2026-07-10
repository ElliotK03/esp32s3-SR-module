---
name: module-progress-eval
description: Evaluate the current progress of a firmware module with a lightweight test harness
---

# Module Progress Evaluation Skill

This skill creates a minimal `main.c` (or `main.cpp`) that can be compiled and run on the host to verify that the module builds and basic functionality works. It is **not** the full firmware; it only includes enough code to exercise the module's public API for testing purposes.

## When to use
- You have added new features or bug fixes to a module and want a quick way to check that it compiles and basic functions behave as expected.
- The full firmware requires hardware, but you need a host‑side test harness.
- You want to generate a temporary test harness without modifying the existing project structure.

## Steps the agent will perform
1. **Detect the module** – Look for a directory under `src/` (or `modules/`) that contains a `CMakeLists.txt` or a `module.h` file.
2. **Create a temporary test directory** `test_harness/` at the project root.
3. **Generate `main.c`** that includes the module's public header and calls a representative function (e.g., `module_init()` or a function annotated with `/** @test */`).
4. **Create a minimal `CMakeLists.txt`** in `test_harness/` that adds the module source files and the generated `main.c`.
5. **Run the build** using the project's build system (e.g., `cmake` + `make`).
6. **Execute the resulting binary** and capture its output.
7. **Report the result** – success if the binary exits with code 0 and prints "PASS"; otherwise show the error output.

## Files created
- `test_harness/main.c` – the generated test harness source.
- `test_harness/CMakeLists.txt` – a minimal CMake file that links the module.

## Example generated `main.c`
```c
#include "<module>/module.h"
#include <stdio.h>

int main(void) {
    if (!module_init()) {
        printf("FAIL: init failed\n");
        return 1;
    }
    printf("PASS\n");
    return 0;
}
```
Replace `<module>` with the actual module directory name.

## Cleanup
After the evaluation, the skill removes the `test_harness/` directory to keep the repository clean.

## Notes
- The skill assumes the project uses CMake; adapt the commands if a different build system is used.
- If the module provides a different entry function, adjust the generated `main.c` accordingly.
- This skill is intended for quick validation, not exhaustive testing.
