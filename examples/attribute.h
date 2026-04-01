#ifndef ATTRIBUTE_H
#define ATTRIBUTE_H

#include "kolibri.h"


#ifndef ATTRIB_MAX_ATTRIBS
	#define ATTRIB_MAX_ATTRIBS 16
#endif

#define ATTRIB_NONE  0xFFFF
#define ATTRIB_MAGIC 0x4C555441  /* "ATLU" */


typedef unsigned short AttributesLUT[ATTRIB_MAX_ATTRIBS];

typedef struct
{
	uint32        magic;
	AttributesLUT lut;
}
AttributeHeader;


#define Attribute_init(header) \
    do { \
        (header).magic = ATTRIB_MAGIC; \
        for (int _i = 0; _i < ATTRIB_MAX_ATTRIBS; _i++) \
            (header).lut[_i] = ATTRIB_NONE; \
    } while(0)

#define Attribute_set(header, attribute, struct_type, field) \
    ((header).lut[attribute] = (unsigned short)offsetof(struct_type, field))

#define Attribute_unset(header, attribute) \
    ((header).lut[attribute] = ATTRIB_NONE)


static inline void *
Attribute_get(Entity *entity, int attribute)
{
	if (!entity->user_data) return NULL;

	AttributeHeader *header = (AttributeHeader*)entity->user_data;
	if (header->magic != ATTRIB_MAGIC) return NULL;

	unsigned short offset = header->lut[attribute];
	if (offset == ATTRIB_NONE) return NULL;

	return (void*)((char*)entity->local_data + offset);
}


#endif /* ATTRIBUTE_H */
