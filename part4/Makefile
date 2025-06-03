# Makefile for Stage 4 - drinks_bar + clients
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17

# Targets
SERVER = drinks_bar
TCP_CLIENT = atom_supplier
UDP_CLIENT = molecule_requester

# Sources
SERVER_SRC = drinks_bar.cpp
TCP_CLIENT_SRC = atom_supplier.cpp
UDP_CLIENT_SRC = molecule_requester.cpp

all: $(SERVER) $(TCP_CLIENT) $(UDP_CLIENT)

$(SERVER): $(SERVER_SRC)
	$(CXX) $(CXXFLAGS) -o $(SERVER) $(SERVER_SRC)

$(TCP_CLIENT): $(TCP_CLIENT_SRC)
	$(CXX) $(CXXFLAGS) -o $(TCP_CLIENT) $(TCP_CLIENT_SRC)

$(UDP_CLIENT): $(UDP_CLIENT_SRC)
	$(CXX) $(CXXFLAGS) -o $(UDP_CLIENT) $(UDP_CLIENT_SRC)

clean:
	rm -f $(SERVER) $(TCP_CLIENT) $(UDP_CLIENT)
