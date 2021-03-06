#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "common.h"
#include "ivector.h"
#include "utils.h"

void ivector_init0(ivector *v)
{
    ivector_init1(v, VECTOR_INIT_CAPACITY);
}

void ivector_init1(ivector *v, int initial_capacity)
{
    v->array = (int*)malloc(initial_capacity * sizeof(int));
    v->size = 0;
    v->capacity = initial_capacity;
}

void ivector_init2(ivector *v, int *x, int size, int initial_capacity)
{
    assert(size <= initial_capacity);
    v->array = (int*)malloc(initial_capacity * sizeof(int));
    v->size = size;
    v->capacity = initial_capacity;

    for (int i = 0; i < size; ++i)
    {
        v->array[i] = x[i];
    }
}

ORIGIN_INLINE void ivector_grow0(ivector *v, int new_capacity)
{
    v->capacity = new_capacity;
    v->array = (int*)realloc(v->array, new_capacity * sizeof(int));
}

ORIGIN_INLINE void ivector_grow1(ivector *v)
{
    // Double the internal storage
    if (v->capacity == 0)
    {
        ivector_grow0(v, VECTOR_INIT_CAPACITY);
    }
    else
    {
        ivector_grow0(v, v->capacity * VECTOR_GROW_RATE);
    }
}

#ifndef NDEBUG
ORIGIN_INLINE int ivector_get(ivector *v, int n)
{
    assert(n < v->size);
    return v->array[n];
}

ORIGIN_INLINE void ivector_set(ivector *v, int n, int x)
{
    assert(n < v->size);
    v->array[n] = x;
}
#endif

ORIGIN_INLINE void ivector_add(ivector *v, int x)
{
    // If the internal array is full, double its size;
    if (v->size == v->capacity)
    {
        ivector_grow1(v);
    }
    // Add the new nent at the end
    v->array[v->size] = x;
    v->size++;
}

ORIGIN_INLINE void ivector_add_array(ivector *v, int *a, int size)
{
    // If there isn't enough space, double the size until it can contain 'a'
    const int old_size = v->size;
    while (v->capacity < old_size + size)
    {
        ivector_grow1(v);
    }
    for (int i = 0; i < size; ++i)
    {
        v->array[old_size + i] = a[i];
    }
    v->size += size;
}

ORIGIN_INLINE void ivector_sub(ivector *v, int x)
{
    if (v->size >= x)
    {
        v->size -= x;
    }
    else
    {
        v->size = 0;
    }
}

ORIGIN_INLINE void ivector_rmvall(ivector *v)
{
    v->size = 0;
}

ORIGIN_INLINE void ivector_sub1(ivector *v)
{
    if (v->size > 0)
    {
        v->size--;
    }
}

ORIGIN_INLINE int *ivector_get_array(ivector *v)
{
    const int size = v->size;
    int *new_array = (int*)malloc(size * sizeof(int)); // Only allocate memory for the active elements

    // Copy the elements to the new array
    for (int i = 0; i < size; ++i)
    {
        new_array[i] = v->array[i];
    }
    return new_array;
}

ORIGIN_INLINE void ivector_sort_asc(ivector *v)
{
    qsort((void*)v->array, v->size, sizeof(int), compare_int_asc);
}

ORIGIN_INLINE void ivector_sort_des(ivector *v)
{
    qsort((void*)v->array, v->size, sizeof(int), compare_int_des);
}

ORIGIN_INLINE int ivector_trim_small(ivector *v, int smallest)
{
    int i = 0;
    int j = 0;
    while (j < v->size)
    {
        if (v->array[j] >= smallest)
        {
            v->array[i] = v->array[j];
            ++i;
        }
        ++j;
    }	
    const int removed = j - i;
    v->size -= removed;
    
    return removed;
}

ORIGIN_INLINE int ivector_trim_large(ivector *v, int largest)
{
    int i = 0;
    int j = 0;
    while (j < v->size)
    {
        if (v->array[j] <= largest)
        {
            v->array[i] = v->array[j];
            ++i;
        }
        ++j;
    }	
    const int removed = j - i;
    v->size -= removed;

    return removed;
}

void ivector_print(ivector *v, FILE *out)
{
    int i = 0;
    for (; i < v->size - 1; ++i)
    {
        fprintf(out, "%d ", v->array[i]);
    }
    fprintf(out, "%d", v->array[i]);
}

ORIGIN_INLINE void ivector_free(ivector *v)
{
    free(v->array); // Free the memory of the vector
    v->array = NULL;
}

