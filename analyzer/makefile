CC=g++ -std=gnu++14
VLROOT=/usr/local/lib/vlfeat-0.9.21/
IDIRS=-I/usr/local/include/opencv -I/usr/local/include/opencv2 -I$(VLROOT)
LDIRS=-L/usr/local/lib/ -L$(VLROOT)bin/glnxa64/ -LD_LIBRARY_PATH$(VLROOT)bin/glnxa64/
LINKS=-g -lvl -lopencv_core -lopencv_imgcodecs -lopencv_imgproc -lopencv_highgui -lopencv_ml -lopencv_features2d -lboost_program_options -lboost_filesystem -pthread

SOURCES := $(wildcard *.cpp) $(wildcard **/*.cpp)
OBJECTS := $(patsubst %.cpp,%.o,$(SOURCES))

segment: $(OBJECTS)
	export LD_LIBRARY_PATH=$(VLROOT)bin/glnxa64
	$(CC) -o segment $(OBJECTS) $(IDIRS) $(LDIRS) $(LINKS)

clean:
	rm -f segment *.o **/*.o


