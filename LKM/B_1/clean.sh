make clean
rm -f a.out
if [ `lsmod | grep mysort | wc -l` -eq 1 ]; then
    sudo rmmod mysort
fi

