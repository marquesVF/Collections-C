/*
 * Collections-C
 * Copyright (C) 2013-2014 Srđan Panić <srdja.panic@gmail.com>
 *
 * This file is part of Collections-C.
 *
 * Collections-C is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Collections-C is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Collections-C.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "array.h"

#define DEFAULT_CAPACITY 8
#define DEFAULT_EXPANSION_FACTOR 2

struct array_s {
    size_t   size;
    size_t   capacity;
    float    exp_factor;
    void   **buffer;

    void *(*mem_alloc)  (size_t size);
    void *(*mem_calloc) (size_t blocks, size_t size);
    void  (*mem_free)   (void *block);
};

static bool expand_capacity(Array *vec);


/**
 * Returns a new empty array, or NULL if the allocation fails.
 *
 * @return a new array if the allocation was successful, or NULL if it was not.
 */
Array *array_new()
{
    ArrayConf c;
    array_conf_init(&c);
    return array_new_conf(&c);
}

/**
 * Returns a new empty array based on the specified ArrayConf struct.
 *
 * The array is allocated using the allocators specified in the ArrayConf
 * struct. The allocation may fail if underlying allocator fails. It may also
 * fail if the values of exp_factor and capacity in the ArrayConf do not meet
 * the following condition: <code>exp_factor < (MAX_ELEMENTS / capacity)</code>.
 *
 * @param[in] conf Array configuration struct. All fields must be initialized to
 *                 appropriate values.
 *
 * @return a new array if the allocation was successful, or NULL if not.
 */
Array *array_new_conf(ArrayConf *conf)
{
    Array *ar = conf->mem_calloc(1, sizeof(Array));

    if (ar == NULL)
        return NULL;

    if (conf->exp_factor <= 1)
        ar->exp_factor = DEFAULT_EXPANSION_FACTOR;

    /* Needed to avoid an integer overflow on the first resize and
     * to easily check for any future oveflows. */
    else if (conf->exp_factor >= MAX_ELEMENTS / conf->capacity)
        return NULL;
    else
        ar->exp_factor = conf->exp_factor;

    ar->capacity   = conf->capacity;
    ar->mem_alloc  = conf->mem_alloc;
    ar->mem_calloc = conf->mem_calloc;
    ar->mem_free   = conf->mem_free;
    ar->buffer     = ar->mem_calloc(ar->capacity, sizeof(void*));

    return ar;
}

/**
 * Initializes the fields of the ArrayConf struct to default values.
 *
 * @param[in, out] conf the struct that is being initialized
 */
void array_conf_init(ArrayConf *conf)
{
    conf->exp_factor = DEFAULT_EXPANSION_FACTOR;
    conf->capacity   = DEFAULT_CAPACITY;
    conf->mem_alloc  = malloc;
    conf->mem_calloc = calloc;
    conf->mem_free   = free;
}

/**
 * Destroys the array structure, but leaves the data it used to hold, intact.
 *
 * @param[in] art the array that is to be destroyed.
 */
void array_destroy(Array *ar)
{
    ar->mem_free(ar->buffer);
    ar->mem_free(ar);
}

/**
 * Destroys the array structure along with all the data it holds.
 *
 * @note
 * This function should not be called on a array that has some of its elements
 * allocated on the stack.
 *
 * @param[in] art the array that is being destroyed
 */
void array_destroy_free(Array *ar)
{
    size_t i;
    for (i = 0; i < ar->size; i++)
        ar->mem_free(ar->buffer[i]);

    array_destroy(ar);
}

/**
 * Adds a new element to the array. The element is appended to the array making
 * it the last element (the one with the highest index) of the array. This
 * function returns a <code>bool</code> based on whether or not the space allocation
 * for the new element was successful or not.
 *
 * @param[in] art the array to which the element is being added
 * @param[in] element the element that is being added
 *
 * @return true if the operation was successful.
 */
bool array_add(Array *ar, void *element)
{
    if (ar->size >= ar->capacity && !expand_capacity(ar))
        return false;

    ar->buffer[ar->size] = element;
    ar->size++;

    return true;
}

/**
 * Adds a new element to the array at a specified position by shifting all
 * subsequent elemnts by one. The specified index must be within the bounds
 * of the array, otherwise this operation will fail and false will be
 * returned to indicate failure. This function may also fail if the space
 * allocation for the new element was unsuccessful.
 *
 * @param[in] art the array to which the element is being added
 * @param[in] element the element that is being added
 * @param[in] index the position in the array at which the element is being
 *            added that must be within bounds of the array
 *
 * @return true if the operation was successful, false if not
 */
bool array_add_at(Array *ar, void *element, size_t index)
{
    if (index > (ar->size - 1))
        return false;

    if (ar->size == ar->capacity && !expand_capacity(ar))
        return false;

    size_t shift = (ar->size - index) * sizeof(void*);

    memmove(&(ar->buffer[index + 1]),
            &(ar->buffer[index]),
            shift);

    ar->buffer[index] = element;
    ar->size++;

    return true;
}

/**
 * Replaces a array element at the specified index and returns the replaced
 * element. The specified index must be within the bounds of the array,
 * otherwise NULL is returned. NULL can also be returned if the replaced element
 * was NULL. In this case calling <code>array_contains()</code> before this
 * function can resolve this ambiguity.
 *
 * @param[in] art the array whose element is being replaced
 * @param[in] element the replacement element
 * @param[in] index the index at which the replacement element should be placed
 *
 * @return the replaced element or NULL if the index was out of bounds.
 */
void *array_replace_at(Array *ar, void *element, size_t index)
{
    if (index >= ar->size)
        return NULL;

    void *old_e = ar->buffer[index];
    ar->buffer[index] = element;

    return old_e;
}

/**
 * Removes and returns the specified element from the array if such element
 * exists. In case the element does not exist NULL is returned. NULL can also be
 * returned if the specified element is NULL. In this case calling <code>
 * array_contains()</code> before this function can resolve this ambiguity.
 *
 * @param[in] art the array from which the element is being removed
 * @param[in] element the element being removed
 *
 * @return the removed element, or NULL if the operation has failed
 */
void *array_remove(Array *ar, void *element)
{
    size_t index = array_index_of(ar, element);

    if (index == NO_SUCH_INDEX)
        return NULL;

    if (index != ar->size - 1) {
        size_t block_size = (ar->size - index) * sizeof(void*);

        memmove(&(ar->buffer[index]),
                &(ar->buffer[index + 1]),
                block_size);
    }
    ar->size--;

    return element;
}

/**
 * Removes and returns a array element from the specified index. The index must
 * be within the bounds of the array, otherwise NULL is returned. NULL may also
 * be returned if the removed element was NULL. To resolve this ambiguity call
 * <code>array_contains()</code> before this function.
 *
 * @param[in] art the array from which the element is being removed
 * @param[in] index the index of the element being removed.
 *
 * @return the removed element, or NULL if the operation fails
 */
void *array_remove_at(Array *ar, size_t index)
{
    if (index >= ar->size)
        return NULL;

    void *e = ar->buffer[index];

    if (index != ar->size - 1) {
        size_t block_size = (ar->size - index) * sizeof(void*);

        memmove(&(ar->buffer[index]),
                &(ar->buffer[index + 1]),
                block_size);
    }
    ar->size--;

    return e;
}

/**
 * Removes and returns a array element from the end of the array. Or NULL if
 * array is empty. NULL may also be returned if the last element of the array
 * is NULL value. This ambiguity can be resolved by calling <code>array_size()
 * </code>before this function.
 *
 * @param[in] the array whose last element is being removed
 *
 * @return the last element of the array ie. the element at the highest index
 */
void *array_remove_last(Array *ar)
{
    return array_remove_at(ar, ar->size - 1);
}

/**
 * Removes all elements from the specified array. This function does not shrink
 * the array capacity.
 *
 * @param[in] art the array from which all elements are to be removed
 */
void array_remove_all(Array *ar)
{
    ar->size = 0;
}

/**
 * Removes and frees all elements from the specified array. This function does
 * not shrink the array capacity.
 *
 * @param[in] art the array from which all elements are to be removed
 */
void array_remove_all_free(Array *ar)
{
    size_t i;
    for (i = 0; i < ar->size; i++)
        free(ar->buffer[i]);

    array_remove_all(ar);
}

/**
 * Returns a array element from the specified index. The specified index must be
 * within the bounds of the array, otherwise NULL is returned. NULL can also be
 * returned if the element at the specified index is NULL. This ambiguity can be
 * resolved by calling <code>array_contains()</code> before this function.
 *
 * @param[in] art the array from which the element is being retrieved
 * @param[in] index the index of the array element
 *
 * @return array element at the specified index, or NULL if the operation has
 *         failed
 */
void *array_get(Array *ar, size_t index)
{
    if (index >= ar->size)
        return NULL;

    return ar->buffer[index];
}

/**
 * Returns the last element of the array ie. the element at the highest index,
 * or NULL if the array is empty. Null may also be returned if the last element
 * of the array is NULL. This ambiguity can be resolved by calling <code>
 * array_size()</code> before this function.
 *
 * @param[in] the array whose last element is being returned
 *
 * @return the last element of the specified array
 */
void *array_get_last(Array *ar)
{
    return array_get(ar, ar->size - 1);
}

/**
 * Returns the underlying array buffer.
 *
 * @note Any direct modification of the buffer may invalidate the array.
 *
 * @param[in] art the array whose underlying buffer is being returned
 *
 * @return the buffer
 */
const void * const*array_get_buffer(Array *ar)
{
    return (const void* const*) ar->buffer;
}

/**
 * Returns the index of the first occurrence of the specified array element, or
 * NO_SUCH_INDEX if the element could not be found.
 *
 * @param[in] art array being searched
 * @param[in] element the element whose index is being looked up
 *
 * @return the index of the specified element, or NO_SUCH_INDEX if the element is not found
 */
size_t array_index_of(Array *ar, void *element)
{
    size_t i;
    for (i = 0; i < ar->size; i++) {
        if (ar->buffer[i] == element)
            return i;
    }
    return NO_SUCH_INDEX;
}

/**
 * Returns a subarray of the specified array, randing from <code>b</code>
 * index (inclusive) to <code>e</code> index (inclusive). The range indices
 * must be within the bounds of the array, while the <code>e</code> index
 * must be greater or equal to the <code>b</code> index. If these conditions
 * not met, NULL is returned.
 *
 * @note The new array is allocated using the original arrays allocators
 *       and also inherits the configuration of the original array.
 *
 * @param[in] ar the array from which the subarray is being returned
 * @param[in] b the beginning index (inclusive) of the subarray that must be
 *              within the bounds of the array and must not exceed the
 *              the end index
 * @param[in] e the end index (inclusive) of the subarray that must be within
 *              the bounds of the array and must be greater or equal to the
 *              beginnig index
 *
 * @return a subarray of the specified array, or NULL
 */
Array *array_subarray(Array *ar, size_t b, size_t e)
{
    if (b > e || e > ar->size)
        return NULL;

    Array *sub_ar     = ar->mem_calloc(1, sizeof(Array));
    sub_ar->mem_alloc  = ar->mem_alloc;
    sub_ar->mem_calloc = ar->mem_calloc;
    sub_ar->mem_free   = ar->mem_free;
    sub_ar->size       = e - b + 1;
    sub_ar->capacity   = sub_ar->size;
    sub_ar->buffer     = ar->mem_alloc(sub_ar->capacity * sizeof(void*));

    memcpy(sub_ar->buffer,
           &(ar->buffer[b]),
           sub_ar->size * sizeof(void*));

    return sub_ar;
}

/**
 * Returns a shallow copy of the specified array. A shallow copy is a copy of
 * the array structure, but not the elements it holds.
 *
 * @note The new array is allocated using the original arrays allocators
 *       and also inherits the configuration of the original array.
 *
 * @param[in] art the array to be copied
 *
 * @return a shallow copy of the specified array
 */
Array *array_copy_shallow(Array *ar)
{
    Array *copy = ar->mem_alloc(sizeof(Array));

    copy->exp_factor = ar->exp_factor;
    copy->size       = ar->size;
    copy->capacity   = ar->capacity;
    copy->buffer     = ar->mem_calloc(copy->capacity, sizeof(void*));
    copy->mem_alloc  = ar->mem_alloc;
    copy->mem_calloc = ar->mem_calloc;
    copy->mem_free   = ar->mem_free;

    memcpy(copy->buffer,
           ar->buffer,
           copy->size * sizeof(void*));

    return copy;
}

/**
 * Returns a deep copy of the specified array. A deep copy is a copy of
 * both the array structure and the data it holds.
 *
 * @note The new array is allocated using the original arrays allocators
 *       and also inherits the configuration of the original array.
 *
 * @param[in] art the array to be copied
 * @param[in] cp the copy function that returns a copy of a array element
 *
 * @return a deep copy of the specified array
 */
Array *array_copy_deep(Array *ar, void *(*cp) (void *))
{
    Array *copy   = ar->mem_alloc(sizeof(Array));

    copy->exp_factor = ar->exp_factor;
    copy->size       = ar->size;
    copy->capacity   = ar->capacity;
    copy->buffer     = ar->mem_calloc(copy->capacity, sizeof(void*));
    copy->mem_alloc  = ar->mem_alloc;
    copy->mem_calloc = ar->mem_calloc;
    copy->mem_free   = ar->mem_free;

    size_t i;
    for (i = 0; i < copy->size; i++)
        copy->buffer[i] = cp(ar->buffer[i]);

    return copy;
}

/**
 * Reverses the order of elements in the specified array.
 *
 * @param[in] art the array that is being reversed.
 */
void array_reverse(Array *ar)
{
    size_t i;
    size_t j;
    for (i = 0, j = ar->size - 1; i < (ar->size - 1) / 2; i++, j--) {
        void *tmp = ar->buffer[i];
        ar->buffer[i] = ar->buffer[j];
        ar->buffer[j] = tmp;
    }
}

/**
 * Trims the array's capacity, in other words, it shrinks the capacity to match
 * the number of elements in the specified array, however the capacity will
 * never shrink below 1.
 *
 * @param[in] art the array whose capacity is being trimmed.
 */
void array_trim_capacity(Array *ar)
{
    if (ar->size == ar->capacity)
        return;

    void   **new_buff = ar->mem_calloc(ar->size, sizeof(void*));
    size_t   size     = ar->size < 1 ? 1 : ar->size;

    memcpy(new_buff, ar->buffer, size * sizeof(void*));
    ar->mem_free(ar->buffer);

    ar->buffer   = new_buff;
    ar->capacity = ar->size;
}

/**
 * Returns the number of occurrences of the element within the specified array.
 *
 * @param[in] art the array that is being searched
 * @param[in] element the element that is being searched for
 *
 * @return the number of occurrences of the element
 */
size_t array_contains(Array *ar, void *element)
{
    size_t o = 0;
    size_t i;
    for (i = 0; i < ar->size; i++) {
        if (element == ar->buffer[i])
            o++;
    }
    return o;
}

/**
 * Returns the size of the specified array. The size of the array is the
 * number of elements contained within the array.
 *
 * @param[in] art the array whose size is being returned
 *
 * @return the the number of element within the array
 */
size_t array_size(Array *ar)
{
    return ar->size;
}

/**
 * Returns the capacity of the specified array. The capacity of the array is
 * the maximum number of elements a array can hold before it has to be resized.
 *
 * @param[in] art array whose capacity is being returned
 *
 * @return the capacity of the array
 */
size_t array_capacity(Array *ar)
{
    return ar->capacity;
}

/**
 * Sorts the specified array.
 *
 * @note
 * Pointers passed to the comparator function will be pointers to the array
 * elements that are of type (void*) ie. void**. So an extra step of
 * dereferencing will be required before the data can be used for comparison:
 * eg. <code>my_type e = *(*((my_type**) ptr));</code>.
 *
 * @param[in] art array to be sorted
 * @param[in] cmp the comparator function that must be of type <code>
 *                int cmp(const void e1*, const void e2*)</code> that
 *                returns < 0 if the first element goes before the second,
 *                0 if the elements are equal and > 0 if the second goes
 *                before the first.
 */
void array_sort(Array *ar, int (*cmp) (const void*, const void*))
{
    qsort(ar->buffer, ar->size, sizeof(void*), cmp);
}

/**
 * Expands the array capacity. This might fail if the the new buffer
 * cannot be allocated. In case the expansion would overflow the index
 * range, a maximum capacity buffer is allocated instead. If the capacity
 * is already at the maximum capacity, no new buffer is allocated and
 * false is returned to indicate the failure.
 *
 * @param[in] art array whose capacity is being expanded
 *
 * @return true if the operation was successful
 */
static bool expand_capacity(Array *ar)
{
    if (ar->capacity == MAX_ELEMENTS)
        return false;

    size_t new_capacity = ar->capacity * ar->exp_factor;

    /* As long as the capacity is greater that the expansion factor
     * at the point of overflow, this is check is valid. */
    if (new_capacity <= ar->capacity)
        ar->capacity = MAX_ELEMENTS;
    else
        ar->capacity = new_capacity;

    void **new_buff = ar->mem_alloc(new_capacity * sizeof(void*));

    if (new_buff == NULL)
        return false;

    memcpy(new_buff, ar->buffer, ar->size * sizeof(void*));

    ar->mem_free(ar->buffer);
    ar->buffer = new_buff;

    return true;
}

/**
 * A 'foreach loop' function that invokes the specified function on each element
 * in the array.
 *
 * @param[in] art the array on which this operation is performed
 * @param[in] op the operation function that is to be invoked on each array
 *               element
 */
void array_foreach(Array *ar, void (*op) (void *e))
{
    size_t i;
    for (i = 0; i < ar->size; i++)
        op(ar->buffer[i]);
}

/**
 * Initializes the iterator.
 *
 * @param[in] iter the iterator that is being initialized
 * @param[in] ar the array to iterate over
 */
void array_iter_init(ArrayIter *iter, Array *ar)
{
    iter->ar   = ar;
    iter->index = 0;
}

/**
 * Checks whether or not the iterator has reached the end of the array
 *
 * @param[in] iter iterator whose position is being checked
 *
 * @return true if there are more elements to be iterated over, or false if not
 */
bool array_iter_has_next(ArrayIter *iter)
{
    return iter->index < iter->ar->size;
}

/**
 * Returns the next element in the sequence and advances the iterator.
 *
 * @param[in] iter the iterator that is being advanced
 *
 * @return the next element in the sequence
 */
void *array_iter_next(ArrayIter *iter)
{
    return iter->ar->buffer[iter->index++];
}

/**
 * Removes and returns the last returned element by <code>array_iter_next()
 * </code> without invalidating the iterator.
 *
 * @param[in] iter the iterator on which this operation is being performed
 *
 * @return the removed element
 */
void *array_iter_remove(ArrayIter *iter)
{
    return array_remove_at(iter->ar, iter->index - 1);
}

/**
 * Adds a new element to the array after the last retuned element by
 * <code>array_iter_next()</code> without invalidating the iterator.
 *
 * @param[in] iter the iterator on which this operation is being performed
 * @parma[in] element the element being added
 *
 * @return true if the element was successfully added or false if the allocation
 *         for the new element failed.
 */
bool array_iter_add(ArrayIter *iter, void *element)
{
    return array_add_at(iter->ar, element, iter->index++);
}

/**
 * Replaces the last returned element by <code>array_iter_next()</code>
 * with the specified replacement element.
 *
 * @param[in] iter the iterator on which this operation is being performed
 * @param[in] element the replacement element
 *
 * @return the old element that was replaced by the new one
 */
void *array_iter_replace(ArrayIter *iter, void *element)
{
    return array_replace_at(iter->ar, element, iter->index);
}

/**
 * Returns the index of the last returned element by <code>array_iter_next()
 * </code>.
 *
 * @note
 * This function should not be called before a call to <code>array_iter_next()
 * </code>
 *
 * @param[in] iter the iterator on which this operation is being performed
 *
 * @return the index
 */
size_t array_iter_index(ArrayIter *iter)
{
    return iter->index - 1;
}
