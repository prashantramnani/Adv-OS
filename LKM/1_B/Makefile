CONFIG_MODULE_SIG=n # workaround for module verification failed

obj-m+=mysort.o

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
	rm a.out

