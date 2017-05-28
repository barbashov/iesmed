LIBS = -lpq -lpqxx -lpthread -L/usr/local/lib
INC = -I. -I./include -I/usr/local/include -pthread
PROG = iesmed
OBJS = $(patsubst %.cpp,%.o,$(wildcard *.cpp))

all: esme

esme: $(OBJS)
	@echo Making iesmed
	$(CXX) -g -Wall $(INC) $(OBJS) $(LIBS) -o $(PROG)


clean:
	@echo Cleaning
	rm -f *.o
	rm -f *.core.*

mrproper: clean
	@echo Cleaning dependency files
	rm -f $(PROG)
	rm -f *.d

%.o: %.cpp
	$(CXX) $(INC) -c -g -MMD $<

#include $(wildcard *.d)
