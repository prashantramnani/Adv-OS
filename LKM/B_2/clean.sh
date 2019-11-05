make clean
rm -f a.out
if [ `lsmod | grep bst | wc -l` -eq 1 ]; then
    sudo rmmod bst
fi

