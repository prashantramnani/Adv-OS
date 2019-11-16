main: main.o disk.o sfs.o
	gcc -o main main.o disk.o sfs.o 
main.o: main.c disk.h sfs.h
	gcc -c -g main.c
sfs.o: sfs.c sfs.h
	gcc -c -g sfs.c
fuse: fuse.o disk.o sfs.o
	gcc -o fuse fuse.o disk.o sfs.o `pkg-config fuse --cflags --libs`
fuse.o: fuse.c disk.h sfs.h
	gcc -c -g fuse.c `pkg-config fuse --cflags --libs`
disk.o: disk.c disk.h
	gcc -c -g disk.c
test_sfs.o: test_sfs.c
	gcc -c -g test_sfs.c
test_sfs: disk.o sfs.o test_sfs.o
	gcc -g -o test_sfs disk.o sfs.o test_sfs.o
test_disk.o: test_disk.c
	gcc -c -g test_disk.c
test_disk: disk.o test_disk.o
	gcc -g -o test_disk disk.o test_disk.o
clean:
	rm -f test_disk test_sfs fuse main *.o
