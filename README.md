Collections-C
=============

A collections library for C.


##Contents
### Examples:
- [List](#list)

#### List

Creating a new list:
```c
List *mylist = list_new();
```

Destroying a list:
```c
list_destroy(mylist); // this frees the list structure but keeps all the data it points to

// to free the list and all the data use
list_destroy_free(mylist); // note that this should not be used if some of the elements are allocated on the stack
```

Adding elements to the list:
```c
list_add(mylist, &my_list_element);
```
Removing element from the list:
```c
list_remove(mylist, &my_list_element);

// or from a specific splace

list_remove_at(mylist, elmenet_index);
```
Iterating over the list in an ascending order:

```c
ListIter *iter = list_iter_new(list);

for (;list_iter_has_next(iter);) {
    // return the next element and advance the iterator
    void *e = list_iter_next(iter);
    do_something(e);
    
    // to safely remove elements during iteration use
    list_iter_remove(iter); // this removes the last returned element by this iterator
    
    // to add an element use
    list_iter_add(iter, element); 
    // this adds the elments just before the elment that would be returned by the ist_iter_next();
}
```
Getting a specific element from the list:

```c
void *my_elmeent = list_get(mylist, element_index); 
```

Sorting the list:

```c
// Since the list contains generic data types, it doesn't actually know how to interpret the data
// that's why we must pass our own comparator function. For example an integer comparator:

int cmp(void *e1, void *e2)
{
    int *i = (int *) e1;
    int *j = (int *) e2;

    if (*i < *j)
        return -1;
    if (*i == *j)
        return 0;
    return 1;
}

// Now you can sort your list of integers
list_sort(mylist, cmp);
```