all:
	g++ src/main.cpp src/Camera.cpp -o webcamTerminal
debug:
	g++ src/main.cpp src/Camera.cpp -o webcamTerminal --debug
clean:
	rm webcamTerminal