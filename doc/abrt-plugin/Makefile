PROGRAM	=	libHelloWorld
CXX	=	g++
SRCS	+=	abrt-reporter-hello-world.cpp
OBJS	+=	$(addsuffix .o, $(basename $(SRCS)))


.PHONY: clean install uninstall

%.o: %.cpp
	$(CXX) `pkg-config --cflags --libs abrt` -m64 -c -Wall -std=c++98 -pedantic $<

$(PROGRAM).so: $(OBJS)
	$(CXX) -shared $< -o $@

clean:
	rm -f *.o *.so

install:
	install -m 755 -D $(PROGRAM).so /usr/lib64/abrt/
	install -m 644 -D HelloWorld.conf /etc/abrt/plugins/

uninstall:
	rm -f /usr/lib64/abrt/$(PROGRAM).so
	rm -f /etc/abrt/plugins/HelloWorld.conf


