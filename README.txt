How to integrate ModularFluids library into an OpenGL application.

First download these two files:
1.	ModularFluids.dll (Debug/Release)
2.	ModularFluids.h

The ModularFluids.dll file must exist in the same folder as the main application’s executable build.
There exists both a debug and release version of the dll file, use the version that corresponds
with the main application’s debug/release configurations and link the dynamic library accordingly.
Then add the ModularFluids.h file to one of the project’s include directories in order to use the
dynamic library’s exposed functions.

IMPORTANT
Before using any of ModularFluid's other API functions, the library's 'LoadLib' function must first be
called to 'sync' ModularFluid's copy of the Glad library to the main application's already initialised
Glad library.
Make sure before attempting to load the ModularFluid's library, that the main application has already
loaded their Glad libary, then when calling LoadLib, pass in a function pointer to the main application's
‘glfwGetProcAddress’ method.
