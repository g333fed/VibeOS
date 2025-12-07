/*
 * ls - list directory contents
 */

#include "../lib/vibe.h"

// VFS node structure (must match kernel)
typedef struct vfs_node {
    char name[64];
    size_t size;
    int is_directory;
    struct vfs_node *parent;
    struct vfs_node *children;
    struct vfs_node *next;
    char *data;
} vfs_node_t;

int main(kapi_t *k, int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : ".";

    void *node = k->open(path);
    if (!node) {
        k->puts("ls: cannot access '");
        k->puts(path);
        k->puts("': No such file or directory\n");
        return 1;
    }

    if (!k->is_dir(node)) {
        // Just print the filename
        k->puts(path);
        k->putc('\n');
        return 0;
    }

    // List directory contents
    vfs_node_t *dir = (vfs_node_t *)node;
    vfs_node_t *child = dir->children;

    while (child) {
        if (child->is_directory) {
            k->set_color(COLOR_CYAN, COLOR_BLACK);
        } else {
            k->set_color(COLOR_WHITE, COLOR_BLACK);
        }
        k->puts(child->name);
        k->set_color(COLOR_WHITE, COLOR_BLACK);
        k->puts("  ");
        child = child->next;
    }
    k->putc('\n');

    return 0;
}
