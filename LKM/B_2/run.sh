#!/bin/bash

make

if [ -f bst.ko ]
then
    sudo insmod bst.ko
    gcc -g test.c
    if [ -f a.out ]
    then
        if [ $# -eq 0 ]
        then
            v=6;
        else
            v=$1;
        fi
        echo $v
        ./a.out $v
    fi
fi
