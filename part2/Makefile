# Makefile - Stage 2: Molecule Supplier & Requester

CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17

TARGET_SERVER = # Makefile - Stage 2: Molecule Supplier & Requester

CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17

TARGET_SERVER = molecule_supplier
TARGET_CLIENT = molecule_requester

all: $(TARGET_SERVER) $(TARGET_CLIENT)

$(TARGET_SERVER): $(TARGET_SERVER).cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET_SERVER) $(TARGET_SERVER).cpp

$(TARGET_CLIENT): $(TARGET_CLIENT).cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET_CLIENT) $(TARGET_CLIENT).cpp

clean:
	rm -f $(TARGET_SERVER) $(TARGET_CLIENT)
