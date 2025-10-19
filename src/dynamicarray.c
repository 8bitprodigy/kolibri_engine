#include <stddef.h>
#include <string.h>

#include "dynamicarray.h"


#define INDEX( a, i )   ((char*)&(a)->data + ((i) *(a)->datum_size))
#define GET_HEADER( a ) (((DynamicArrayHeader*)((char*)(a) - offsetof(DynamicArrayHeader, data))))


typedef struct
{
	size_t        length;
	size_t        capacity;
	size_t        datum_size;
	unsigned char data[];
}
DynamicArrayHeader;


/*******************************
    Constructor / Destructor
*******************************/
void *
DynamicArray_new( size_t datum_size, size_t capacity )
{
	DynamicArrayHeader *array   = malloc(sizeof(DynamicArrayHeader) + (datum_size * capacity));
	if (!array) {
		DBG_OUT("Failed to allocate DynamicArray.");
		return NULL;
	}
	
	array->datum_size = datum_size;
	array->length     = 0;
	array->capacity   = capacity;

	return (void*)&array->data;
} /* DynamicArray_new */


void 
DynamicArray_free(void *self)
{
	free(GET_HEADER(self));
} /* DynamicArray_free */


void
DynamicArray_grow(void **self)
{
    DynamicArrayHeader *old_header = GET_HEADER(*self);
    
    size_t new_capacity = DYNAMIC_ARRAY_GROWTH_FACTOR * old_header->capacity;
    size_t new_size = sizeof(DynamicArrayHeader) + (new_capacity * old_header->datum_size);
    
    DynamicArrayHeader *new_header = realloc(old_header, new_size);
    if (!new_header) {
        DBG_OUT("Failed to allocate new grown array.");
        return;
    }
    
    new_header->capacity = new_capacity;
    *self = (void*)&new_header->data;
} /* DynamicArray_grow */


void
DynamicArray_shrink(void *self)
{
	DynamicArrayHeader *header = GET_HEADER(self);
	
	void *new_arr = realloc(
			header, 
			(
				(header->length + 1) 
				* header->datum_size
			) + sizeof(DynamicArrayHeader)
		);
	if (!new_arr) {
		DBG_OUT("Failed to allocate new shrunk array.");
		return;
	}

	header = new_arr;
	header->capacity = header->length+1;
} /* DynamicArray_shrink */


void
DynamicArray_append(void **self, void *data, size_t size)
{
	DynamicArrayHeader *header = GET_HEADER(*self);
	while (header->capacity <= header->length + size) {
		DynamicArray_grow(self);
		header = GET_HEADER(*self);
	}

	memcpy(INDEX(header, header->length), data, size * header->datum_size);
	header->length += size;
} /* DynamicArray_append */

size_t
DynamicArray_capacity(void *self)
{
	return GET_HEADER(self)->capacity;
} /* DynamicArray_capacity */

void
DynamicArray_clear(void *self)
{
	GET_HEADER(self)->length = 0;
}

void
DynamicArray_concat(void **self, void *array)
{
	DynamicArrayHeader *arr_hdr = GET_HEADER(array);
	DynamicArray_append(self, &arr_hdr->data, arr_hdr->length);
} /* DynamicArray_concat */


void
DynamicArray_delete(void *self, size_t index, size_t size)
{
	DynamicArrayHeader *header = GET_HEADER(self);
	
	int rest = header->length - index;
	memcpy(INDEX(header, index), INDEX(header, index + size), rest * header->datum_size);
	header->length -= size;
} /* DynamicArray_delete */


void
DynamicArray_insert(void **self, size_t index, void *data, size_t size)
{
	DynamicArrayHeader *header = GET_HEADER(*self);
	
	while (header->capacity <= header->length + size) {
		DynamicArray_grow(self);
		header = GET_HEADER(*self);
	}

	int rest = header->length - index;
	memcpy(INDEX(header, index+size), INDEX(header, index), rest * header->datum_size);
	memcpy(INDEX(header, index), data, size * header->datum_size);
	header->length += size;
} /* DynamicArray_insert */


size_t
DynamicArray_length(void *self)
{
	return GET_HEADER(self)->length;
} /* DynamicArray_length */

void
DynamicArray_replace(void **self, size_t index, void *data, size_t size)
{
	DynamicArrayHeader *header = GET_HEADER(*self);
	
	size_t amount = index + size;
	while (header->capacity <= amount) {
		DynamicArray_grow(self);
		header = GET_HEADER(*self);
	}
	
	header = GET_HEADER(*self);
	memcpy(INDEX(header, index), data, size * header->datum_size);
	header->length = (header->length < amount) ? amount : header->length;
} /* DynamicArray_replace */
