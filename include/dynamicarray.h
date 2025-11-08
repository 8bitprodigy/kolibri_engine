#ifndef BTCHWRK_DYNAMIC_ARRAY_H
#define BTCHWRK_DYNAMIC_ARRAY_H


#include "common.h"


#ifndef DYNAMIC_ARRAY_GROWTH_FACTOR
	#define DYNAMIC_ARRAY_GROWTH_FACTOR 2.0f
#endif

#define DynamicArray(type_, capacity) DynamicArray_new(sizeof(type_), capacity)
#define DynamicArray_add(array, datum) DynamicArray_append((array), (datum), 1)


/* Constructor / Destructor */
void   *DynamicArray_new(      size_t   datum_size, size_t capacity);
void    DynamicArray_free(     void    *array);

/* Methods */
void    DynamicArray_grow(     void   **array);
void    DynamicArray_shrink(   void    *array);

void    DynamicArray_append(   void   **array,  void    *data,  size_t  length);
size_t  DynamicArray_capacity( void    *array);
void    DynamicArray_clear(    void    *array);
void    DynamicArray_concat(   void   **array1, void   *array2);
void    DynamicArray_delete(   void    *array,  size_t   index, size_t length);
void    DynamicArray_insert(   void   **array,  size_t   index, void   *data,   size_t length);
size_t  DynamicArray_length(   void    *array);
void    DynamicArray_replace(  void   **array,  size_t   index, void   *data,   size_t length);


#endif /* BTCHWRK_DYNAMIC_ARRAY_H */
