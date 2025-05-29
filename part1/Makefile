# Makefile - Stage 1: Atom Warehouse and Supplier

CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17

TARGET_SERVER = atom_warehouse
TARGET_CLIENT = atom_supplier

all: $(TARGET_SERVER) $(TARGET_CLIENT)

$(TARGET_SERVER): $(TARGET_SERVER).cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET_SERVER) $(TARGET_SERVER).cpp

$(TARGET_CLIENT): $(TARGET_CLIENT).cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET_CLIENT) $(TARGET_CLIENT).cpp

clean:
	rm -f $(TARGET_SERVER) $(TARGET_CLIENT)
