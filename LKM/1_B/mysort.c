#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <asm/current.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/sort.h>
#include <linux/string.h>
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
};

static struct list_node{
    struct list_head process_list;
    struct info_node info;
};

LIST_HEAD(head);
//static struct list_head head = LIST_INIT_HEAD(head);

#define RW_PERM 0777
#define TYPE_INT 0xff
#define TYPE_STRING 0xf0

int cmp_int(const void *a, const void *b)
{
    return *(int *)a - *(int *)b;
}

int cmp_str(const void *a, const void *b)
{
    return strcmp((char *)a, (char *)b);
}

static void create_new_entry(int pid)
{
    struct list_node *entry;
    entry = kmalloc(sizeof(struct list_node *), GFP_KERNEL);

    entry->info.pid = pid;
    entry->info.writep = 0;
    entry->info.readp = 0;

    entry->info.type = UNDEFINED;
    list_add(&entry->process_list, &head);

    printk(KERN_ALERT "Created new entry for pid = %d\n", pid);
}

static void fill_entry(struct list_node *entry, const char *buf, size_t count)
{
    entry->info.size = (int)buf[1];

    if (buf[0] == (char)TYPE_INT) {
        entry->info.type = INT;
        entry->info.data.intp = (int *)kmalloc(sizeof(int) * entry->info.size, GFP_KERNEL);
    } else if (buf[0] == (char)TYPE_STRING) {
        entry->info.type = STRING;
        entry->info.data.charp = (char **)kmalloc(sizeof(char *) * entry->info.size, GFP_KERNEL);
    }

    printk(KERN_ALERT "Filled entry for pid = %d\n", current->pid);
}

static int open(struct inode *inodep, struct file *filep)
{
    create_new_entry(current->pid);
    return 0;
}

static ssize_t write(struct file *file, const char *buff, size_t count, loff_t *pos)
{
    printk(KERN_ALERT "Write from process pid = %d\n", current->pid);
    int pid = current->pid;

    char *buf = (char *)kmalloc(sizeof(char) * count, GFP_KERNEL);
    // error handling
    copy_from_user(buf, buff, count);
    printk(KERN_ALERT "writing %s\n", buf);

    struct list_head *ptr;
    bool found = false;

    list_for_each(ptr, &head) {
        struct list_node *entry = list_entry(ptr, struct list_node, process_list);

        if (entry->info.pid == pid) {
            if (entry->info.type == UNDEFINED) {
                fill_entry(entry, buf, count);
            } else if (entry->info.type == INT) {
                int val;
                kstrtoint(buf, 10, &val);

                // error handling
                entry->info.data.intp[entry->info.writep++] = val;
                printk(KERN_ALERT "Wrote %d\n", val);

                if (entry->info.writep == entry->info.size) {
                    sort(entry->info.data.intp, entry->info.size, sizeof(int), cmp_int, NULL);
                }
            } else if (entry->info.type == STRING) {
                // error handling
                entry->info.data.charp[entry->info.writep++] = buf;
                printk(KERN_ALERT "charp-> %s\n", entry->info.data.charp[entry->info.writep-1]);
                if (entry->info.writep == entry->info.size) {
                    sort(entry->info.data.charp, entry->info.size, sizeof(char) * MAX_STR_SIZE, cmp_str, NULL);
                }
            }
            // error handling

            return entry->info.size - entry->info.writep;
        }
    }

    printk(KERN_ALERT "Not found pid = %d\n", pid);
    // error handling
    return -1; // entry->size
}

static ssize_t read(struct file *file, char *buf, size_t count, loff_t *pos)
{
    // error handling
    int pid = current->pid;
    struct list_head *ptr;

    list_for_each(ptr, &head) {
        struct list_node *entry = list_entry(ptr, struct list_node, process_list);

        if (entry->info.pid == pid) {
            // error handling

            if (entry->info.type == INT) {
                copy_to_user(buf, entry->info.data.intp[entry->info.readp++], sizeof(int));
            } else if (entry->info.type == STRING) {
                copy_to_user(buf, entry->info.data.charp[entry->info.readp++], sizeof(char) * MAX_STR_SIZE);
            } else {
                // error handling
            }
        }
        return 0; // return size;
    }

    //error handling
    return -1;
}


static struct file_operations file_ops = {
    .owner = THIS_MODULE,
    .open = open,
    .write = write,
    .read = read,
};

static int mysort_init(void)
{
    printk(KERN_ALERT "Mysort module activated");
    struct proc_dir_entry *entry = proc_create("partb_1_15CS30002", RW_PERM, NULL, &file_ops);
    if (!entry)
    {
        printk(KERN_ALERT "-enoent from init\n");
        return -ENOENT;}
    return 0;
}

static void mysort_exit(void)
{
    printk(KERN_ALERT "exiting mysort\n");
    remove_proc_entry("partb_1_15CS30002", NULL);
}


module_init(mysort_init);
module_exit(mysort_exit);

