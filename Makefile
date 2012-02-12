test: test.cpp Makefile space.hpp
	g++ -W -Wall -Wextra -Wconversion -std=c++0x -ggdb3 -O0 test.cpp
	#clang -std=c++0x test.cpp -W -Wall -Wextra  -c