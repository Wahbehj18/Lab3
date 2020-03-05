default:
	gcc -Wall -Wextra -o lab3a lab3a.c -lm
dist:
	tar -cvzf lab3a-105114897.tar.gz lab3a.c lab3a Makefile README 
clean:
	rm -f lab3a lab3a-105114897.tar.gz
