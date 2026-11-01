CXX      = g++
CXXFLAGS = -std=c++23 -Wall -Wextra -O2

all: twmailer-server twmailer-client

twmailer-server: twmailer-server.cpp common.h
	$(CXX) $(CXXFLAGS) -o $@ $<

twmailer-client: twmailer-client.cpp common.h
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f twmailer-server twmailer-client
