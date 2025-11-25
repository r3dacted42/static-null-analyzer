# --- Compiler and Flags ---

CXX = g++

# CXXFLAGS:
# -g:         Enable debug symbols
# -Wall:      Enable all standard warnings
# -Wextra:    Enable extra warnings
# -MMD -MP:   Auto-generate dependency files (.d)
CXXFLAGS = -std=c++23 -g -Wall -Wextra -MMD -MP \
        	-fsanitize=address,undefined -fno-omit-frame-pointer

# CPPFLAGS (C Pre-Processor Flags):
# -I: Add include paths for your headers
CPPFLAGS = -I./src/include \
           -I./src/cfg \
           -I./src/dot \
           -I./src/dfa

# --- Project Files ---

# The name of your final executable
TARGET = analyzer

# Find all .cpp files to compile
SRCS = src/main.cpp \
       src/cfg/cfg.cpp \
       src/dot/dot.cpp \
       src/dfa/dfa.cpp

# Generate corresponding object file names (e.g., src/main.o)
OBJS = $(SRCS:.cpp=.o)

# Generate dependency file names (e.g., src/main.d)
DEPS = $(OBJS:.o=.d)

# --- Build Rules ---

# Default target: build the executable
.PHONY: all
all: $(TARGET)

# Rule to link the final executable
$(TARGET): $(OBJS)
	@echo "Linking executable: $@"
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

# Generic rule to compile a .cpp file into a .o file
# $< is the source file (e.g., src/main.cpp)
# $@ is the target file (e.g., src/main.o)
# $(CPPFLAGS) provides the -I include paths
%.o: %.cpp
	@echo "Compiling: $<"
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@


# Clean up all build artifacts
.PHONY: clean
clean:
	@echo "Cleaning up..."
	rm -f $(TARGET) $(OBJS) $(DEPS)
	@echo "Done."

# Include the generated dependency files
# The '-' allows 'make' to continue if they don't exist (e.g., on first build)
-include $(DEPS)