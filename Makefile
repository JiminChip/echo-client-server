all: echo-client.cpp echo-server.cpp mingw_net.cpp
	g++ -o echo-client echo-client.cpp mingw_net.cpp
	g++ -o echo-server echo-server.cpp mingw_net.cpp

echo-client: echo-client.cpp mingw_net.cpp
	 g++ -o echo-client echo-client.cpp mingw_net.cpp

echo-server: echo-server.cpp mingw_net.cpp
	 g++ -o echo-server echo-server.cpp mingw_net.cpp

clean:
	rm -f echo-client echo-server