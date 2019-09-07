#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include<sys/wait.h>

#define MAX_INT 100

int cmp_int(const void * a, const void * b)
{
    return ( *(int*)a - *(int*)b );
}

int cmp_str(const void * a, const void * b)
{
    return strcmp((const char *)a, (const char *)b);
}

int _open(){
    int fd;
    fd = open("/proc/partb_1_15CS30002", O_RDWR);
    if(fd < 0) {
        printf("errror opening file\n");
        return -1;
    }
    return fd;
}

int test_integers()
{
    int fd;
    if ((fd = _open()) < 0) return fd;
    // Testing Integer objects
    int N = 20;
    // write metadata
    char buf[2];
    buf[0]= (char)0xFF;
    buf[1] = (char)N;

    if(write(fd, buf,strlen(buf)) < 0) {
        printf("Write for metadata failed\n");
        close(fd);
        return -1;
    }

    int i, flg = 0;
    int arr[N];
    printf("Writing numbers to LKM\n");
    for (i = 0; i < N; i++) {
        int val = rand() % MAX_INT;
        arr[i] = val;

        if (write(fd, &arr[i], sizeof(int)) != (N - i - 1)) {
            printf("Unexpected behaviour from write\n");
            flg = -1;
            break;
        }
    }

    if (flg == 0) {
        printf("Writes completed\n");
        printf("Array of size %d generated below: \n", N);
        for (int i = 0; i < N; i++)
            printf("%d ", arr[i]);
        printf("\n\nReading numbers from LKM\n");

        qsort(arr, N, sizeof(int), cmp_int);

        for (i = 0; i < N; i++) {
            int v;
            if (read(fd, &v, sizeof(int)) < 0) {
                printf("Unexpected behaviour from read\n");
                break;
            };
            if (arr[i] != v) {
                printf("Sorting order wrong from LKM: i = %d, expected = %d, found =  %d", i, arr[i], v);
                break;
            }
            if (i ==(N - 1))
                printf("Sorted output matched\n");
        }
    }
    close(fd);
    return 0;
}

int test_strings()
{
    int N = 6;
    int fd;

    if ((fd = _open()) < 0) return fd;
    // write metadata
    char buf[2];
    buf[0]= (char)0xF0;
    buf[1] = (char)N;

    if(write(fd, buf,strlen(buf)) < 0) {
        printf("Write for metadata failed\n");
        close(fd);
        return -1;
    }

    int i, flg = 0;
    char arr[N][100];
    printf("Writing strings to LKM\n");
    for (i = 0; i < N; i++) {
        scanf("%100s", arr[i]);

        if (write(fd, &arr[i], strlen(arr[i])) != (N - i - 1)) {
            printf("Unexpected behaviour from write\n");
            flg = -1;
            break;
        }
    }

    if (flg == 0) {
        printf("Writes completed\n");
        printf("Given Array of size %d below: \n", N);
        for (int i = 0; i < N; i++)
            printf("%s\n", arr[i]);
        printf("\n\nReading strings from LKM\n");

        qsort(arr, N, 100, cmp_str);

        for (i = 0; i < N; i++) {
            char buff[100];

            if (read(fd, &buff, 100) < 0) {
                printf("Unexpected behaviour from read\n");
                break;
            };
            printf("%d %s\n", i, buff);
            if (strcmp(buff, arr[i]) != 0) {
                printf("Sorting order wrong from LKM:, expected = %s, found = %s\n", arr[i], buff);
                break;
            }
            if (i ==(N - 1))
                printf("Sorted output matched\n");
        }
    }

    close(fd);
    return 0;
}

int test_concurrency()
{
    int pid = fork(), ret = 0;
    if (pid != 0) {
        printf("Testing from Parent with pid = %d\n", getpid());
        ret = test_integers();
        wait(NULL);
    } else {
        printf("Testing from Child with pid = %d\n", getpid());
        ret = test_integers();
    }

    return ret;
}


int main(int argc, char **argv)
{
    int val = 2;
    if (argc > 1)
        val = atoi(argv[1]);
    printf("Val = %d\n", val);

    if (val % 2 == 0)
        if (test_integers() < 0)
            printf("Integers test failed\n");
    if (val % 3 == 0)
        if (test_strings() < 0)
            printf("Strings test failed\n");
    if (val % 5 == 0)
        if (test_concurrency() < 0)
            printf("Concurrecy test failed\n");
    return 0;
}

