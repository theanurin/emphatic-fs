/**
 *  inode_table.c
 *
 *  Implementation of the procedures declared in inode_table.h.
 *
 *  Author: Matthew Signorini
 */

#include "const.h"
#include "fat.h"
#include "inode_table.h"


// local functions.
PRIVATE file_list_t ** get_inode (const file_list_t **list, 
  fat_entry_t inode);


/**
 *  Create a new entry in an active i-node list, with exactly one 
 *  reference.
 */
    PUBLIC void
ilist_add (list, fd)
    file_list_t **list;     // list to add the item to.
    fat_file_t *fd;         // file (and i-node) to add.
{
    file_list_t *new_item = safe_malloc (sizeof (file_list_t));

    // fill in the newly allocated structure.
    new_item->file = fd;
    new_item->refcount = 1;
    new_item->next = *list;

    // link the new item onto the head of the list.
    *list = new_item;
}

/**
 *  look up a given i-node value in a list of active i-nodes. If an entry
 *  is found, this function will fill in the fat_file_t pointer at the
 *  address given by the second parameter; if no entry is found, the
 *  pointer will be left unchanged.
 *
 *  Return value is true if an entry is found, and false otherwise.
 */
    PUBLIC bool
ilist_lookup_file (list, fd, inode)
    const file_list_t **list;       // list to search.
    fat_file_t **fd;                // file handle pointer to fill in.
    fat_entry_t inode;              // i-node to search for.
{
    file_list_t **found = get_inode (list, inode);

    // If get_list returned an entry, we have found a match.
    if (*found != NULL)
    {
        // found a match. Fill in the file handle, and return true.
        *fd = (*found)->file;
        (*found)->refcount += 1;
        return true;
    }

    // no match found.
    return false;
}

/**
 *  Decrement the reference count of the list entry matching a given
 *  i-node, and remove the entry when the reference count reaches zero.
 */
    PUBLIC void
ilist_unlink (list, inode)
    file_list_t **list;     // list to search for the item.
    fat_entry_t inode;      // key to search for.
{
    file_list_t **item = get_inode (list, inode), temp;
    fat_file_t *fd;
    cluster_list_t *prev = NULL, *cp;

    // check that we found an item.
    if (*item == NULL)
        return;

    // decrement the reference count.
    if (((*item)->refcount -= 1) != 0)
        return;

    // If we reach this point, then we have just removed the last
    // reference, so we need to remove the item from the active i-nodes
    // list.
    temp = *item;
    *item = (*item)->next;
    fd = temp->file;
    safe_free (&temp);

    // Free the memory used by the file structure, including the file
    // name, and list of clusters.
    safe_free (&(fd->name));

    for (cp = fd->clusters; cp != NULL; cp = cp->next)
    {
        // release the item before the current item, as the current item
        // is referenced by the loop update statement.
        if (prev != NULL)
            safe_free (&prev);

        prev = cp;
    }

    // free the last item, and then the file struct.
    safe_free (&prev);
    safe_free (&fd);
}

/**
 *  Search an active i-nodes list for an entry matching a given i-node.
 *
 *  Return value is a pointer to the matching item, if found, or a pointer
 *  to a NULL pointer if no match is found.
 */
    PRIVATE file_list_t **
get_inode (list, inode)
    const file_list_t **list;       // list to search.
    fat_entry_t inode;              // key to search for.
{
    // traverse the list until we find a matching item.
    //
    // \begin{voodoo}
    for ( ; (INODE (*list) != inode) && (*list != NULL); 
      list = &((*list)->next))
    {
        ;
    }

    return list;
    // \end{voodoo}
}


// vim: ts=4 sw=4 et
