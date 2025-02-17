LDFLAGS = -lgpiodcxx

all: ir_replay

ir_replay: main.cpp
	$(CXX) -o ir_replay main.cpp $(LDFLAGS)

