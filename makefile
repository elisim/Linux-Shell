# All Targets
all: myshell

# Tool invocations
myshell: myshell.o LineParser.o 
	gcc -g -Wall -o myshell bin/myshell.o bin/LineParser.o 
	echo build success! 

# Depends on the source and header files
LineParser.o: src/LineParser.c include/LineParser.h
	gcc -g -Wall -ansi -c src/LineParser.c -o bin/LineParser.o

myshell.o: src/myshell.c
	gcc -g -Wall -ansi -c src/myshell.c -o bin/myshell.o

#tell make that "clean" is not a file name!
.PHONY: clean run

#Clean the build directory	
clean: 
	rm -vf myshell bin/*
run:
	./myshell