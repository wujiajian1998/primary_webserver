CXX = g++
CFLAGS = -std=c++14 -O2 -Wall -g 

TARGET = server
OBJS = ./log/*.cpp  ./timer/*.cpp \
       ./http_conn/*.cpp   *.cpp \
       ./buffer/*.cpp  

all: $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) -o $(TARGET)  -pthread 

clean:
	rm -rf $(OBJS) $(TARGET)

