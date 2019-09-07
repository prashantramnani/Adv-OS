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
MODULE_DESCRIPTION("Loadable kernel module for operations on binary search tree");

MODULE_VERSION("0.1");

#define MAX_STR_SIZE 101

#define PB2_SET_TYPE _IOW(0x10, 0x31, int32_t*)
#define PB2_SET_ORDER _IOW(0x10, 0x32, int32_t*)
#define PB2_GET_INFO _IOR(0x10, 0x33, int32_t*)
#define PB2_GET_OBJ _IOR(0x10, 0x34, int32_t*)

enum data_type {
    INT = (char)0xff,
    STRING = (char)0xf0,
    UNDEFINED = -2,
};

enum order {
    INORDER = (char)0x00,
    PREORDER = (char)0x01,
    POSTORDER = (char)0x02,
};

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

static struct bst_node {
    union data_un{
        int i_key;
        char str_key[MAX_STR_SIZE];
    }data;

    struct bst_node *left, *right;
};

static struct info_node{
    int pid;
    enum data_type type;
    enum order ord;
    int readp;
    int num_elem;

    struct bst_node *root;
    struct info_node *next;
};

static struct info_node *head = NULL;;
DEFINE_MUTEX(lock);

static struct info_node* get_entry(int pid)
{
    struct info_node* ptr;
    for (ptr = head; ptr != NULL; ptr = ptr->next)
        if (ptr->pid == pid)
            return ptr;

    printk(KERN_INFO "No entry found for pid = %d", pid);
    return NULL;
}

static int create_new_entry(int pid)
{
    struct info_node *entry;
    entry = (struct info_node *)kmalloc(sizeof(struct info_node), GFP_KERNEL);

    entry->pid = pid;

    entry->type = UNDEFINED;
    entry->ord = INORDER;
    entry->readp = 0;
    entry->num_elem = 0;
    entry->root =  NULL;
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

static int open(struct inode *inodep, struct file *filep)
{
    printk(KERN_INFO "Open called");
    return create_new_entry(current->pid);
}

void delete_bst(struct bst_node* nd)
{
    if (!nd) return;
    delete_bst(nd->left);
    delete_bst(nd->right);

    kfree((void *)nd);
}

void delete_entry(int pid)
{
    mutex_lock(&lock);
    struct info_node *ptr, *ptr2;

    for (ptr = head, ptr2 = NULL; ptr != NULL; ptr = ptr->next) {
        if (ptr->pid == pid) {
            delete_bst(ptr->root);

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
    printk(KERN_INFO "DUMMY");
    return 0;
}

// Generic insert: struct bst_node* insert(void *key){}

struct bst_node* insert_int(struct bst_node* nd, int p)
{
    if (!nd) {
        nd = (struct bst_node*) kmalloc(sizeof(struct bst_node), GFP_KERNEL);
        nd->data.i_key = p;
        nd->left = nd->right = NULL;

        return nd;
    }

    if (p < nd->data.i_key)
        nd->left = insert_int(nd->left, p);
    else
        nd->right = insert_int(nd->right, p);

    return nd;
}

struct bst_node* insert_str(struct bst_node* nd, char p[])
{
    if (!nd) {
        nd = (struct bst_node*) kmalloc(sizeof(struct bst_node), GFP_KERNEL);
        strcpy(nd->data.str_key, p);
        nd->left = nd->right = NULL;

        return nd;
    }

    if (strcmp(p, nd->data.str_key) < 0)
        nd->left = insert_str(nd->left, p);
    else nd->right = insert_str(nd->right, p);

    return nd;
}

static ssize_t write(struct file *file, const char *buff, size_t count, loff_t *pos)
{
    printk(KERN_INFO "Write from process pid = %d", current->pid);

    mutex_lock(&lock);
    struct info_node *entry = get_entry(current->pid);
    if (!entry || (entry->type == UNDEFINED)) {
        mutex_unlock(&lock);
        return -EBADF;
    }

    if (entry->type == INT) {
        int val2;
        copy_from_user(&val2, buff, sizeof(int));

        printk(KERN_INFO "Inserting int val = %d", val2);
        entry->root = insert_int(entry->root, val2);
    } else if (entry->type == STRING) {
        char buffer[MAX_STR_SIZE];
        copy_from_user(buffer, buff, count + 1);

        printk(KERN_INFO "Inserting string val = %d", buffer);
        entry->root = insert_str(entry->root, buffer);
    } else {
        mutex_unlock(&lock);
        return -EINVAL;
    }

    entry->num_elem++;
    entry->readp = 0;
    mutex_unlock(&lock);
    return 0;
}

void get_nth_h(struct bst_node* nd, char *buf, enum data_type ty, int rp, int *cnt)
{
    (*cnt)++;
    if (*cnt == rp) {
        if (ty == INT)
            copy_to_user(buf, &(nd->data.i_key), sizeof(int));
        //snprintf(buf, sizeof(int), "%d", nd->data.i_key);
        else if (ty == STRING)
            copy_to_user(buf, nd->data.str_key, strlen(nd->data.str_key) + 1);

        return;
    }
}

void get_nth(struct bst_node* nd, int rp, enum data_type ty, char *buf, int *cnt, enum order ord)
{
    if (!nd) return;
    if (*cnt < rp) {
        if (ord == PREORDER) get_nth_h(nd, buf, ty, rp, cnt);
        get_nth(nd->left, rp, ty, buf, cnt, ord);
        if (ord == INORDER) get_nth_h(nd, buf, ty, rp, cnt);
        get_nth(nd->right, rp, ty, buf, cnt, ord);
        if (ord == POSTORDER) get_nth_h(nd, buf, ty, rp, cnt);
    }
}

static ssize_t read(struct file *file, char *buf, size_t count, loff_t *pos)
{
    mutex_lock(&lock);
    struct info_node *entry = get_entry(current->pid);
    if (!entry) {
        mutex_unlock(&lock);
        return -EINVAL;
    }

    if (entry->readp >= entry->num_elem) {
        mutex_unlock(&lock);
        return 0;
    }

    int cnt = 0;
    get_nth(entry->root, entry->readp + 1, entry->type, buf, &cnt, entry->ord);
    entry->readp++;

    mutex_unlock(&lock);
    return strlen(buf);
}

int set_type(struct info_node *entry, unsigned long arg)
{
    char buff;
    // use access_ok() to check for validity of arg.
    copy_from_user(&buff, (char *)arg, sizeof(char));

    entry->type = UNDEFINED;
    entry->ord = INORDER;
    entry->readp = 0;
    entry->num_elem = 0;
    delete_bst(entry->root);
    entry->root =  NULL;
    entry->next = NULL;

    if (buff == INT || buff == STRING)
        entry->type = buff;
    else {
        printk(KERN_INFO "Invalid object type!");
        return -EINVAL;
    }

    return 0;
}

int set_order(struct info_node* entry, unsigned long arg)
{
    char buff;
    copy_from_user(&buff, (char *)arg, sizeof(char));

    if (buff == INORDER || buff == PREORDER || buff == POSTORDER)
        entry->ord = buff;
    else {
        printk(KERN_INFO "Invalid order!");
        return -EINVAL;
    }
    entry->readp = 0;
    return 0;
}

void trav_inorder(struct bst_node* nd, struct obj_info* inf, char par, int H)
{
    if (!nd) return;
    int h = H;
    if (par == 'T') h++;
    trav_inorder(nd->left, inf, 'T', h);

    if (!nd->left && !nd->right) {
        inf->maxdepth = max(inf->maxdepth, H);
        inf->mindepth = min(inf->mindepth, H);
    }

    int cnt = 0;
    if (par == 'T') cnt++;
    if (nd->left) cnt++;
    if (nd->right) cnt++;

    if (cnt == 3)inf->deg3cnt++;
    if (cnt == 2) inf->deg2cnt++;
    if (cnt == 1) inf->deg1cnt++;

    trav_inorder(nd->right, inf, 'T', h);
}

int get_bst_info(struct info_node* entry, unsigned long arg)
{
    struct obj_info *inf = (struct obj_info *)kmalloc(sizeof(struct obj_info), GFP_KERNEL);
    copy_from_user(inf, (struct obj_info *)arg, sizeof(struct obj_info));

    inf->deg1cnt = inf->deg2cnt = inf->deg3cnt = inf->maxdepth = 0, inf->mindepth = entry->num_elem + 1;
    trav_inorder(entry->root, inf, 'F', 0);
    if (inf->mindepth == (entry->num_elem + 1)) inf->mindepth = 0;
    copy_to_user((struct obj_info *)arg, inf, sizeof(struct obj_info));

    kfree((void *)inf);
    return entry->num_elem;
}

void bst_search(struct bst_node* nd, struct search_obj *obj)
{
    if (!nd) return;
    if (obj->objtype == INT) {
        if (nd->data.i_key == obj->int_obj)
            obj->found = 0;
        else if (nd->data.i_key < obj->int_obj)
            bst_search(nd->right, obj);
        else bst_search(nd->left, obj);
    } else if (obj->objtype == STRING) {
        int res = strncmp(nd->data.str_key, obj->str, obj->len);
        if (res == 0)
            obj->found = 0;
        else if (res < 0)
            bst_search(nd->right, obj);
        else bst_search(nd->left, obj);
    }
}

void search(struct info_node* entry, unsigned long arg)
{
    struct search_obj *obj = (struct search_obj *)kmalloc(sizeof(struct search_obj), GFP_KERNEL);
    copy_from_user(obj, (struct search_obj *)arg, sizeof(struct search_obj));

    obj->found = 1;
    if (obj->objtype != entry->type) return;
    printk(KERN_INFO "Searching");
    bst_search(entry->root, obj);
    copy_to_user((struct search_obj *)arg, obj, sizeof(struct search_obj));
    kfree((void *)obj);
}

static long ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    mutex_lock(&lock);
    struct info_node* entry = get_entry(current->pid);
    if (entry == NULL) {
        mutex_unlock(&lock);
        return -EINVAL;
    }

    int ret;
    switch (cmd) {
        case PB2_SET_TYPE:
            ret = set_type(entry, arg);
            break;

        case PB2_SET_ORDER:
            ret =set_order(entry, arg);
            break;

        case PB2_GET_INFO:
            ret = get_bst_info(entry, arg);
            break;

        case PB2_GET_OBJ:
            search(entry, arg);
            ret = 0;
            break;

        default:
            printk(KERN_INFO "invalid objtype");
            ret = -EINVAL;
            break;
    }
    mutex_unlock(&lock);
    return ret;
}

static struct file_operations file_ops = {
    .open = open,
    .write = write,
    .read = read,
    .release = release,
    .unlocked_ioctl = ioctl,
};

static int bst_init(void)
{
    printk(KERN_INFO "bst module activated");
    struct proc_dir_entry *entry = proc_create("partb_2_15CS30002", S_IRUGO | S_IWUGO, NULL, &file_ops);
    if (entry == NULL)
        return -ENOENT;
    return 0;
}

static void bst_exit(void)
{
    remove_proc_entry("partb_2_15CS30002", NULL);
    printk(KERN_INFO "exit bst");
}


module_init(bst_init);
module_exit(bst_exit);

