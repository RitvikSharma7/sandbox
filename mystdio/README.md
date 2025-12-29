# Project Overview
* Custom stdio-like library
* Implements buffering policies and formatted output

## Features
* Fully buffered, line buffered, unbuffered modes
* Custom putc, puts, printf (minimal version of printf - supports %c, %d, %%, %s)
* Signal-safe flushing behavior
* EINTR-safe writes

## Design Decisions
* Unbuffered streams prioritize correctness over syscall minimization
* Buffer flush semantics modeled after POSIX stdio
* Layered architecture (printf -> puts -> putc -> write)

## Limitations
* No read support just yet for time sake
* Thread safety not implemented
* Locale/width specifiers omitted
