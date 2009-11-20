CXX=g++
CXXFLAGS=-Wall -Wextra -Werror -pedantic -ansi -g -msse
INCLUDE=
LIBDIRS=
LDFLAGS=-lalut $(LIBDIRS)

SOURCES=sound.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=sound

all: $(SOURCES) $(EXECUTABLE) 

$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CXX) -c $(CXXFLAGS) $(INCLUDE) $< -o $@

clean:
	rm -rf *.o $(EXECUTABLE)
