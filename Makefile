all: myshell

myshell: myshell.c
	gcc -g -o myshell myshell.c -lreadline
