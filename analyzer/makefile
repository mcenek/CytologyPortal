CC=g++ -std=gnu++14
VLROOT=/usr/local/lib/vlfeat-0.9.21/
IDIRS=-I/usr/local/include/opencv -I/usr/local/include/opencv2 -I$(VLROOT)
LDIRS=-L/usr/local/lib/ -L$(VLROOT)bin/glnxa64/ -LD_LIBRARY_PATH$(VLROOT)bin/glnxa64/
LINKS=-g -lvl -lopencv_core -lopencv_imgcodecs -lopencv_imgproc -lopencv_highgui -lopencv_ml -lopencv_features2d -lboost_program_options -lboost_filesystem

%.o: %.cpp
	$(CC) -c -o $@ $< $(IDIRS) $(LDIRS) $(LINKS)

segment: segment.o Segmenter.cpp VLFeatWrapper.cpp functions/LSM.o functions/SegmenterTools.o functions/ClumpSegmentation.o functions/EvaluateSegmentation.o functions/InitialCellSegmentation.o functions/NucleiDetection.o functions/OverlappingCellSegmentation.o objects/Cell.o objects/Clump.o objects/Image.o
	export LD_LIBRARY_PATH=$(VLROOT)bin/glnxa64
	[ -d ../images ] || mkdir ../images #TODO: Consider just adding ../images to the initial repo
	$(CC) -o segment segment.o functions/*.o objects/*.o $(IDIRS) $(LDIRS) $(LINKS)

clean:
	rm -f segment segment.o **/*.o


