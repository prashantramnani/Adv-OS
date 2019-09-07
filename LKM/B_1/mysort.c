#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <asm/current.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/sort.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/slab.h>         // kmalloc()

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Harsha, Ranjith");
MODULE_DESCRIPTION("Loadable kernel module for sorting either strings or ints");

MODULE_VERSION("0.1");

#define MAX_USERS 100
#define MAX_STR_SIZE 100

enum data_type {
    INT = 0,
    STRING,
    UNDEFINED,
};

static struct info_node{
    int pid ;
    int type;
    int size;
    union data_un{
        int* intp;
        char **charp;
    }data;
    int readp;
    int writep;

    struct info_node *next;
};

static struct info_node *head = NULL;;
DEFINE_MUTEX(lock);

#define RW_PERM 0777
#define TYPE_INT 0xff
#define TYPE_STRING 0xf0

static int cmp_int(const void *a, const void *b)
{
    return *(int *)a - *(int *)b;
}

static int cmp_str(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

static int create_new_entry(int pid)
{
    struct info_node *entry;
    entry = (struct info_node *)kmalloc(sizeof(struct info_node), GFP_KERNEL);

    entry->pid = pid;
    entry->writep = 0;
    entry->readp = 0;

    entry->type = UNDEFINED;
    entry->next = NULL;

    mutex_lock(&lock);
    if (head == NULL) {
        head = entry;
    } else {
        struct info_node *p = head;
        int flg = 1;
        while (p->next != NULL) {
            if (p->pid == pid) {
                flg = 0;
                break;
            }
            p = p->next;
        }
        if (!flg || (p->pid == pid)) {
            printk(KERN_INFO "Entry already exists for pid = %d", pid);
            mutex_unlock(&lock);
            return -1;
        }

        p->next = entry;
    }
    mutex_unlock(&lock);

    printk(KERN_INFO "Created new entry for pid = %d", pid);
    return 0;
}

static int fill_entry(struct info_node *entry, const char *buf)
{
    entry->size = (int)buf[1];

    if (buf[0] == (char)TYPE_INT) {
        entry->type = INT;
        entry->data.intp = (int *)kmalloc(sizeof(int) * entry->size, GFP_KERNEL);
    } else if (buf[0] == (char)TYPE_STRING) {
        entry->type = STRING;
        entry->data.charp = (char **)kmalloc(sizeof(char *) * entry->size, GFP_KERNEL);

        int i;
        for (i = 0; i < entry->size; i++)
            entry->data.charp[i] = (char *) kmalloc(sizeof(char) * MAX_STR_SIZE, GFP_KERNEL);
    } else {
        printk(KERN_INFO "Object type invalid!");
        return -EINVAL;
    }

    printk(KERN_INFO "Filled entry for pid = %d, size = %d, type = %d", current->pid, entry->size, entry->type);
    return 0;
}

static int open(struct inode *inodep, struct file *filep)
{
    printk(KERN_INFO "Open called");
    return create_new_entry(current->pid);
}

void delete_entry(int pid)
{
    mutex_lock(&lock);
    struct info_node *ptr, *ptr2;

    for (ptr = head, ptr2 = NULL; ptr != NULL; ptr = ptr->next) {
        if (ptr->pid == pid) {
            if (ptr->type == INT)
                kfree((void *)ptr->data.intp);
            else if (ptr->type == STRING) {
                // free all strings
                int i;
                for (i = 0; i < ptr->size; i++)
                    kfree((void *)ptr->data.charp[i]);
                kfree((void *) ptr->data.charp);
            }

            if (ptr2 == NULL) head = ptr->next;
            else
                ptr2->next = ptr->next;

            kfree((void *)ptr);

            printk(KERN_INFO "Freed up entry for pid = %d" , pid);
            // Todo: multiple files per process
            break;
        }
        ptr2 = ptr;
    }
    mutex_unlock(&lock);
}

static int release(struct inode *inodep, struct file *filep){
    delete_entry(current->pid);
    printk(KERN_INFO "File successfully closed");
    return 0;
}

static ssize_t write(struct file *file, const char *buff, size_t count, loff_t *pos)
{
    printk(KERN_INFO "Write from process pid = %d", current->pid);
    int pid = current->pid;

    mutex_lock(&lock);
    struct info_node *entry;

    for (entry = head; entry != NULL; entry = entry->next) {
        if (entry->pid == pid) {
            int ret = 0;
            if (entry->type == UNDEFINED) {
                char buf[2];
                copy_from_user(buf, buff, 2 * sizeof(char));

                ret = fill_entry(entry, buf);
            } else if (entry->writep == entry->size){
                ;// ignore
            } else if (entry->type == INT) {
                // error handling
                copy_from_user(&(entry->data.intp[entry->writep]), buff, sizeof(int));
                entry->writep++;
                ret = entry->size - entry->writep;
                if (entry->writep == entry->size) {
                    printk(KERN_INFO "Sorting numbers");
                    sort(entry->data.intp, entry->size, sizeof(int), &cmp_int, NULL);
                }
            } else if (entry->type == STRING) {
                copy_from_user(entry->data.charp[entry->writep], buff, count + 1);
                printk(KERN_INFO "charp-> %s", entry->data.charp[entry->writep]);

                entry->writep++;
                ret = entry->size - entry->writep;
                if (entry->writep == entry->size){
                    printk(KERN_INFO "Sorting strings");
                    sort(entry->data.charp, entry->size, sizeof(char *), &cmp_str, NULL);
                }
            } else {
                printk(KERN_INFO "Invalid argument found!");
                ret = -EINVAL;
            }
            mutex_unlock(&lock);
            return ret;
        }
    }

    mutex_unlock(&lock);

    printk(KERN_INFO "Not found pid = %d", pid);
    return -EBADF;
}

static ssize_t read(struct file *file, char *buf, size_t count, loff_t *pos)
{
    // error handling
    int pid = current->pid;
    struct info_node *entry;

    mutex_lock(&lock);
    for (entry = head; entry != NULL; entry = entry->next) {
        if (entry->pid == pid) {
            // error handling
            int ret = 0;
            if (entry->writep < entry->size) {
                printk(KERN_INFO "Can't read until all writes finish");
                ret = -EACCES;
            }
            if (entry->readp == entry->size) {
                printk(KERN_INFO "No element left to read");
                ret = -1;
            }
            if (entry->type == INT) {
                copy_to_user(buf, &(entry->data.intp[entry->readp]), sizeof(int));
            } else if (entry->type == STRING) {
                copy_to_user(buf, entry->data.charp[entry->readp], strlen(entry->data.charp[entry->readp]) + 1);
            }

            printk(KERN_INFO "COPIED buf = %s, %d, readp = %d", buf, strlen(buf), entry->readp);
            entry->readp++;
            mutex_unlock(&lock);
            return ret;
        }
    }
    mutex_unlock(&lock);

    printk(KERN_INFO "No entry found for pid = %d", pid);
    return -1;
}


static struct file_operations file_ops = {
    .open = open,
    .write = write,
    .read = read,
    .release = release,
};

static int mysort_init(void)
{
    printk(KERN_INFO "Mysort module activated");
    struct proc_dir_entry *entry = proc_create("partb_1_15CS30002", S_IRUGO | S_IWUGO, NULL, &file_ops);
    if (entry == NULL)
        return -ENOENT;
    return 0;
}

static void mysort_exit(void)
{
    remove_proc_entry("partb_1_15CS30002", NULL);
    printk(KERN_INFO "exit mysort");
}


module_init(mysort_init);
module_exit(mysort_exit);

