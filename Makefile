CXX = g++
CXXFLAGS = -std=c++17 -O2 -pthread -Iinclude
SRC = src
OBJ = build

all: server client

server: $(SRC)/server.cpp $(SRC)/kv_store.cpp
	$(CXX) $(CXXFLAGS) -o cachify_server $(SRC)/server.cpp $(SRC)/kv_store.cpp

client: $(SRC)/client.cpp
	$(CXX) $(CXXFLAGS) -o cachify_client $(SRC)/client.cpp

clean:
	rm -f cachify_server cachify_client
