TARGETS = sikradio-sender sikradio-receiver test-receiver

CC     = g++
CXXFLAGS = -Wall -O2 -std=c++11
LDLIBS = -lpthread
LDLIBS += -lboost_program_options

all: $(TARGETS)

err.o: err.h

time-driven-thread.o: time-driven-thread.hpp

sikradio-sender.o sikradio-receiver.o: err.h message-driven-thread.hpp time-driven-thread.hpp

sikradio-sender: sikradio-sender.o err.o

sikradio-receiver: sikradio-receiver.o err.o

test-receiver: test-receiver.o


.PHONY: clean

clean:
	rm -f $(TARGETS) *.o *~ *.bak
