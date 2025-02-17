all:
	g++ src/main.cpp src/Camera.cpp -o webcamTerminal
debug:
	g++ --debug src/main.cpp src/Camera.cpp -o webcamTerminal
clean:
	rm webcamTerminal