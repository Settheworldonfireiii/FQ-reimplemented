# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.23

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/local/Cellar/cmake/3.23.2/bin/cmake

# The command to remove a file.
RM = /usr/local/Cellar/cmake/3.23.2/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /Users/radkevii/Documents/work/cumulus_barcode/FQ-reimplemented

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /Users/radkevii/Documents/work/cumulus_barcode/FQ-reimplemented

# Include any dependencies generated for this target.
include CMakeFiles/FQ_reimplemented.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/FQ_reimplemented.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/FQ_reimplemented.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/FQ_reimplemented.dir/flags.make

CMakeFiles/FQ_reimplemented.dir/main.cpp.o: CMakeFiles/FQ_reimplemented.dir/flags.make
CMakeFiles/FQ_reimplemented.dir/main.cpp.o: main.cpp
CMakeFiles/FQ_reimplemented.dir/main.cpp.o: CMakeFiles/FQ_reimplemented.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/Users/radkevii/Documents/work/cumulus_barcode/FQ-reimplemented/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/FQ_reimplemented.dir/main.cpp.o"
	/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/FQ_reimplemented.dir/main.cpp.o -MF CMakeFiles/FQ_reimplemented.dir/main.cpp.o.d -o CMakeFiles/FQ_reimplemented.dir/main.cpp.o -c /Users/radkevii/Documents/work/cumulus_barcode/FQ-reimplemented/main.cpp

CMakeFiles/FQ_reimplemented.dir/main.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/FQ_reimplemented.dir/main.cpp.i"
	/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /Users/radkevii/Documents/work/cumulus_barcode/FQ-reimplemented/main.cpp > CMakeFiles/FQ_reimplemented.dir/main.cpp.i

CMakeFiles/FQ_reimplemented.dir/main.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/FQ_reimplemented.dir/main.cpp.s"
	/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /Users/radkevii/Documents/work/cumulus_barcode/FQ-reimplemented/main.cpp -o CMakeFiles/FQ_reimplemented.dir/main.cpp.s

# Object files for target FQ_reimplemented
FQ_reimplemented_OBJECTS = \
"CMakeFiles/FQ_reimplemented.dir/main.cpp.o"

# External object files for target FQ_reimplemented
FQ_reimplemented_EXTERNAL_OBJECTS =

FQ_reimplemented: CMakeFiles/FQ_reimplemented.dir/main.cpp.o
FQ_reimplemented: CMakeFiles/FQ_reimplemented.dir/build.make
FQ_reimplemented: CMakeFiles/FQ_reimplemented.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/Users/radkevii/Documents/work/cumulus_barcode/FQ-reimplemented/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable FQ_reimplemented"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/FQ_reimplemented.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/FQ_reimplemented.dir/build: FQ_reimplemented
.PHONY : CMakeFiles/FQ_reimplemented.dir/build

CMakeFiles/FQ_reimplemented.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/FQ_reimplemented.dir/cmake_clean.cmake
.PHONY : CMakeFiles/FQ_reimplemented.dir/clean

CMakeFiles/FQ_reimplemented.dir/depend:
	cd /Users/radkevii/Documents/work/cumulus_barcode/FQ-reimplemented && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /Users/radkevii/Documents/work/cumulus_barcode/FQ-reimplemented /Users/radkevii/Documents/work/cumulus_barcode/FQ-reimplemented /Users/radkevii/Documents/work/cumulus_barcode/FQ-reimplemented /Users/radkevii/Documents/work/cumulus_barcode/FQ-reimplemented /Users/radkevii/Documents/work/cumulus_barcode/FQ-reimplemented/CMakeFiles/FQ_reimplemented.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/FQ_reimplemented.dir/depend

