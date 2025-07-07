# Run

Run all tests:

    pixi r all

Serve html:

    pixi r serve

# Motivation

The POSIX API has over 1,000 functions. The WASI API has around 60. WASI/WASM
is more modern API, with security and sandboxing as primary concerns. This
"deny by default" approach neccessitates a smaller, more carefuly curated set
of APIs to minimize the potential attack surface.

Our motivation is in a simpler, yet powerful API and to make it easy to export
programs to the web as well as any platform with minimal overhead.

We use WASM/WASI as our API, and build a lean subset of the C standard library
on top. C programs can use most of the C standard library as usual, and
everything will just work. We also provide extensions, such as arena allocator,
vector / dict data structures, etc.

Then on the backend side we implement JavaScript driver to be able to call such
a program, as well as native implementation to be able to run the program
natively. It runs in wasmtime/wasmer out of the box.

Compared to emscripten, we tightly control the API, so there are no unnecessary calls back and forth with JavaScript.
