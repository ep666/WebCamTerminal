all:
	g++ -std=c++17 src/main.cpp src/Camera.cpp src/Params.cpp -o webcamTerminal -Wall
debug:
	g++ -std=c++17 --debug src/main.cpp src/Camera.cpp src/Params.cpp -o webcamTerminal -Wall
clean:
	rm webcamTerminal