# --- Compiler and Flags ---

CXX = g++

# CXXFLAGS:
# -g:         Enable debug symbols
# -Wall:      Enable all standard warnings
# -Wextra:    Enable extra warnings
# -MMD -MP:   Auto-generate dependency files (.d)
CXXFLAGS = -std=c++23 -g -Wall -Wextra -MMD -MP

# CPPFLAGS (C Pre-Processor Flags):
# -I: Add include paths for your headers
CPPFLAGS = -I./src/include \
           -I./src/cfg \
           -I./src/dot

# --- Project Files ---

# The name of your final executable
TARGET = analyzer

# Find all .cpp files to compile
SRCS = src/main.cpp \
       src/cfg/cfg.cpp \
       src/dot/dot.cpp

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

# --- Utility Rules ---

# Clean up all build artifacts
.PHONY: clean
clean:
	@echo "Cleaning up..."
	rm -f $(TARGET) $(OBJS) $(DEPS) graph.dot graph.png
	@echo "Done."

# Run the analyzer on your test file
# This assumes 'tests/00.json' is the pre-filtered JSON input
.PHONY: run
run: all
	@echo "Running analyzer on tests/00.json..."
	./$(TARGET) tests/00.json
	@echo "Analyzer finished. Output saved to graph.dot"

# Generate the graph.png from the graph.dot output
# Requires 'graphviz' (sudo apt-get install graphviz)
.PHONY: graph
graph: graph.dot
	@echo "Generating graph.png from graph.dot..."
	dot -Tpng graph.dot -o graph.png
	@echo "Graph saved to graph.png"

# A helper to run the analysis, generate the graph, and open it
.PHONY: view
view: run graph
	@echo "Opening graph.png..."
	# Use xdg-open on Linux, or 'open' on macOS
	xdg-open graph.png 2>/dev/null || open graph.png

# Include the generated dependency files
# The '-' allows 'make' to continue if they don't exist (e.g., on first build)
-include $(DEPS)