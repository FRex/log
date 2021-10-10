# LOG

Simple multi threaded logger. Work in progress. Do not use yet. For now
uses `pthread` and GCC atomics and thread locals, the `__atomic` and
`__thread` built-in ones (they also work on Clang). This might change (soon).

See `log.h` for API and `main.c` for usage example.
