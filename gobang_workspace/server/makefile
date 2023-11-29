gobang:gobang_server.cc
	g++ $^ -o $@ -L/usr/lib64/mysql -lmysqlclient -lpthread -lboost_system -ljsoncpp -std=c++11

.PHONY:clean
clean:
	rm -f gobang