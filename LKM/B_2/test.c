#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include<sys/wait.h>

#define MAX_STR_SIZE 100
#define FILE_NAME "/proc/partb_2_15CS30002"
#define PB2_SET_TYPE _IOW(0x10, 0x31, int32_t*)
#define PB2_SET_ORDER _IOW(0x10, 0x32, int32_t*)
#define PB2_GET_INFO _IOR(0x10, 0x33, int32_t*)
#define PB2_GET_OBJ _IOR(0x10, 0x34, int32_t*)

struct obj_info {
    int32_t deg1cnt;
    int32_t deg2cnt;
    int32_t deg3cnt;
    int32_t maxdepth;
    int32_t mindepth;
};

struct search_obj {
    char objtype;
    char found;
    int32_t int_obj;
    char str[MAX_STR_SIZE];
    int32_t len;
};


int _open(){
    int fd;
    fd = open(FILE_NAME, O_RDWR);
    if(fd < 0) {
        printf("errror opening file\n");
        return -1;
    }
    return fd;
}

int test_integers() {
    int fd;
    if ((fd = _open()) < 0) return fd;

    char buf = 0xff;
    if (ioctl(fd, PB2_SET_TYPE, (void *)&buf) < 0) {
        printf("Error in setting type\n");
        return -1;
    }

    // buf = 0x01; // preorder
    //buf = 0x02; // postorder
    buf = 0x00;

    if (ioctl(fd, PB2_SET_ORDER, (void *)&buf) < 0) {
        printf("Error in setting order\n");
        return -1;
    }

    int N = 8, i;
    for (i = 0; i < N; i++) {
        int val = rand() % 100;
        // char buff[4];
        // sprintf(buff, "%d", val);

        printf("value = %d\n", val);
        if (write(fd, &val, sizeof(int)) < 0) {
            printf("Error in write\n");
            break;
        }
    }

    printf("Reads from LKM:\n");
    for (int i = 0; i < N; i++) {
        int v;
        if (read(fd, &v, sizeof(int)) < 0) {
            printf("Unexpected behaviour from read\n");
            break;
        }
        printf("i = %d, val = %d\n", i, v);
    }

    char buff[4];
    printf("read after trav : %d\n", read(fd, buff, 4));

    struct search_obj so;
    so.objtype = 0xff;
    printf("Enter int to search: ");
    scanf("%d", &so.int_obj);
    ioctl(fd, PB2_GET_OBJ, &so);

    if (so.found == 0) printf("Found\n");
    else printf("Not found\n");

    struct search_obj so1;
    so1.objtype = 0xff;
    so1.int_obj = 99;
    ioctl(fd, PB2_GET_OBJ, &so1);

    if (so1.found == 0) printf("Found\n");
    else printf("Not found\n");

    struct obj_info inf;
    ioctl(fd, PB2_GET_INFO, &inf);
    printf("deg1cnt = %d\n"
            "deg2cnt = %d\n"
            "deg3cnt = %d\n"
            "maxdepth = %d\n"
            "mindepth = %d\n",
            inf.deg1cnt, inf.deg2cnt, inf.deg3cnt, inf.maxdepth, inf.mindepth);
    close(fd);
    return 0;
}

int test_strings() {
    int fd;
    if ((fd = _open()) < 0) return fd;

    char buf = 0xf0;
    if (ioctl(fd, PB2_SET_TYPE, (void *)&buf) < 0) {
        printf("Error in setting type\n");
        return -1;
    }

    // buf = 0x01; // preorder
    // buf = 0x02; // postorder
    buf = 0x00;
    if (ioctl(fd, PB2_SET_ORDER, (void *)&buf) < 0) {
        printf("Error in setting order\n");
        return -1;
    }

    int N = 5, i;
    printf("Enter %d strings\n", N);
    for (i = 0; i < N; i++) {
        char str[MAX_STR_SIZE];
        scanf("%100s", str);
        if (write(fd, str, strlen(str)) < 0) {
            printf("Error in write\n");
            break;
        }
    }

    printf("Reads from LKM:\n");
    for (int i = 0; i < N; i++) {
        char buff[100];
        if (read(fd, buff, 4) < 0) {
            printf("Unexpected behaviour from read\n");
            break;
        }
        printf("i = %d, str = %s\n", i, buff);
    }

    char buff[100];
    printf("read after trav : %d\n", read(fd, buff, 4));

    struct search_obj so;
    so.objtype = 0xf0;
    strcpy(so.str, "haha");
    ioctl(fd, PB2_GET_OBJ, &so);

    printf("Found = %c\n", so.found);

    struct search_obj so1;
    so1.objtype = 0xf0;
    strcpy(so.str, "hehe");
    ioctl(fd, PB2_GET_OBJ, &so1);

    printf("Found = %c\n", so1.found);

    struct obj_info inf;
    ioctl(fd, PB2_GET_INFO, &inf);
    printf("deg1cnt = %d\n"
            "deg2cnt = %d\n"
            "deg3cnt = %d\n"
            "maxdepth = %d\n"
            "mindepth = %d\n",
            inf.deg1cnt, inf.deg2cnt, inf.deg3cnt, inf.maxdepth, inf.mindepth);
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

