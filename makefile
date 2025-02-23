all:
	g++ src/main.cpp src/Camera.cpp src/Params.cpp -o webcamTerminal
debug:
	g++ --debug src/main.cpp src/Camera.cpp src/Params.cpp -o webcamTerminal
clean:
	rm webcamTerminal