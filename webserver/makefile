CXX ?= g++

DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2
	
endif

server: main.cpp http_conn.cpp server.cpp lst_time.cpp log.cpp sql_conn.cpp
	$(CXX) -o server  $^ $(CXXFLAGS) -lpthread -L/usr/lib64/mysql -lmysqlclient -lpthread

clean:
	rm  -r server