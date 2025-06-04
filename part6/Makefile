# Makefile for part5
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17

# Targets
SERVER = drinks_bar
SUPPLIER = atom_supplier
REQUESTER = molecule_requester

# Sources
SERVER_SRC = drinks_bar.cpp
SUPPLIER_SRC = atom_supplier.cpp
REQUESTER_SRC = molecule_requester.cpp

all: $(SERVER) $(SUPPLIER) $(REQUESTER)

$(SERVER): $(SERVER_SRC)
	$(CXX) $(CXXFLAGS) -o $(SERVER) $(SERVER_SRC)

$(SUPPLIER): $(SUPPLIER_SRC)
	$(CXX) $(CXXFLAGS) -o $(SUPPLIER) $(SUPPLIER_SRC)

$(REQUESTER): $(REQUESTER_SRC)
	$(CXX) $(CXXFLAGS) -o $(REQUESTER) $(REQUESTER_SRC)

clean:
	rm -f $(SERVER) $(SUPPLIER) $(REQUESTER) *.txt *.sock

