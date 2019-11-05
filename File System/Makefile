main: main.o disk.o sfs.o
	gcc -o main main.o disk.o sfs.o 
main.o: main.c disk.h sfs.h
	gcc -c -g main.c
sfs.o: sfs.c sfs.h
	gcc -c -g sfs.c
disk.o: disk.c disk.h
	gcc -c -g disk.c
test_disk.o: test_disk.c
	gcc -c -g test_disk.c
test_disk: disk.o test_disk.o
	gcc -g -o test_disk disk.o test_disk.o
clean:
	rm -f test_disk *.o
