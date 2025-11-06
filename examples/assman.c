#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "assman.h"


#define BUCKET_COUNT 37


typedef struct
Asset
{
	void         *data;
	void         *release_data;
	AssReleaseFn  Release;
	size_t        ref_count;
}
Asset;

typedef struct
AssNode
{
	struct AssNode 
					*parent,
					*child,
					*prev,
					*next;
	Asset           *asset;
	char             fragment[];
}
AssNode;

typedef struct
AssType
{
	char           *extension;
	AssLoaderFn     Load;
	AssReleaseFn    Release;
	struct AssType *next;
}
AssType;

struct
AssMan
{
	AssNode *root;
	AssType *filetype_buckets[BUCKET_COUNT];
};


static void AssNode_free_siblings(AssNode *);
static void AssNode_free(AssNode *);


static size_t
hash_extension(const char *ext) 
{
	/* Skip leading '.' if present */
	if (ext[0] == '.') ext++;
	if (!ext[0]) return BUCKET_COUNT - 1; /* Empty -- goes in the "other" bucket */

	char c = tolower(ext[0]);

	if ('a' <= c && c <= 'z')
		return c - 'a';          /* Buckets 0-25 */
	else if ('0' <= c && c <= '9')
		return 26 + (c - '0');   /* Buckets 26-35 */
	else
		return BUCKET_COUNT - 1; /* "other" bucket for odd extensions */
}

static size_t
longest_common_prefix(const char *a, const char *b)
{
	size_t i = 0;
	while (
		a[i] && b[i] 
		&& a[i] == b[i]
	) i++;

	return i;
}

static char *
strdup_local(const char *s) {
    size_t  len = strlen(s) + 1;
    char   *d   = malloc(len);
    if (d) memcpy(d, s, len);
    return d;
}



static Asset *
Asset_new(void *data, AssReleaseFn releaser, void *release_data)
{
	Asset *asset        = malloc(sizeof(Asset));
	asset->data         = data;
	asset->ref_count    = 1;
	asset->Release      = releaser;
	asset->release_data = release_data;

	return asset;
}

void
Asset_free(Asset *self)
{
	free(self);
}


static AssNode *
AssNode_new(const char *fragment, Asset *asset)
{
	size_t   length = strlen(fragment);
	AssNode *node   = malloc( sizeof(AssNode) + length + 1);

    node->parent = NULL;
	node->child  = NULL;
    node->prev   = NULL;
	node->next   = NULL;
	node->asset  = asset;

	strcpy(node->fragment, fragment);
	return node;
}

static void
AssNode_free_siblings(AssNode *first)
{
    while (first) {
        AssNode *next = first->next;
        AssNode_free(first);
        first = next;
    }
}

static void
AssNode_free(AssNode *self)
{
	if (!self) return;
	
	AssNode_free_siblings(self->child);

	if (self->asset) {
		Asset_free(self->asset);
	}
	
	free(self);
}

static AssNode *
AssNode_walk(AssNode *self, const char *fragment)
{
	while (self) {
		size_t 
			match    = longest_common_prefix(self->fragment, fragment),
			frag_len = strlen(self->fragment);

		if (match == 0) {
			/* No common prefix -- try sibling */
			self = self->next;
			continue;
		}

		/* Partial match, node longer than path */
		if (match < frag_len) return NULL; 

		fragment += match; /* Exact match */
		if (*fragment == '\0') return self;

		self = self->child;
	}

	return NULL;
}

static AssNode *
AssNode_insert(AssNode **self, const char *path, Asset *asset)
{
	AssNode *node = *self;

	if (!node) {
		*self = AssNode_new(path, asset);
		return *self;
	}

	size_t
		match    = longest_common_prefix(node->fragment, path),
		frag_len = strlen(node->fragment),
		path_len = strlen(path);

	if (match == 0) {
		/* No common prefix -- try sibling */
		AssNode *result = AssNode_insert(&node->next, path, asset);
		if (node->next && !node->next->prev)
			node->next->prev = node;
		return result;
	}

	if (match < frag_len) {
		/* Split node -- shortest first. */
		AssNode *child = AssNode_new(node->fragment + match, node->asset);
		child->child = node->child;

		if (child->child)
			child->child->parent = child;

		node->fragment[match] = '\0';
		node->child           = child;
		child->parent         = node;
		node->asset           = NULL;

		if (match == path_len) {
			/* New asset ends here */
			node->asset = asset;
			return node;
		}
		else {
			AssNode *new_node = AssNode_new(path + match, asset);
			new_node->next    = node->child;
			new_node->parent  = node;
			
			node->child->prev = new_node;
			node->child       = new_node;
			return new_node;
		}
	}

	if (match == path_len) {
		/* Exact match -- assign asset */
		node->asset = asset;
		return node;
	}

	/* Full match of this fragment, Continue with recursive insertion */
	AssNode *result = AssNode_insert(&node->child, path + match, asset);
	if (node->child && !node->child->parent)
		node->child->parent = node;
		
	return result;
}

static void
AssNode_unlink(AssNode *node, AssMan *assman)
{
    if (!node) return;
    
    if (node->prev) {
        node->prev->next = node->next;
    } else if (node->parent) {
        node->parent->child = node->next;
    } else {
        assman->root = node->next;
    }
    
    if (node->next) {
        node->next->prev = node->prev;
    }
}


/******************
	A S S M A N
******************/
/*
	Constructor / Destructor
*/
AssMan *
AssMan_new(void)
{
	AssMan *assman = malloc(sizeof(AssMan));
	if (!assman) return NULL;

	assman->root = NULL;
	for (size_t i = 0; i < BUCKET_COUNT; i++)
        assman->filetype_buckets[i] = NULL;

	return assman;
}

void
AssMan_free(AssMan *assman)
{
	if (!assman) return;
	AssMan_clear(assman);
	free(assman);
}

/*
	Methods
*/
AssType *
AssMan__findFiletype(AssMan *assman, const char *ext)
{
	size_t ext_hash = hash_extension(ext);

	AssType *ass_type = assman->filetype_buckets[ext_hash];

	while(ass_type) {
		if (strcasecmp(ext, ass_type->extension) == 0)
			return ass_type;
		ass_type = ass_type->next;
	}
	return NULL;
}

void
AssMan_registerFiletype(
	AssMan       *assman,
	const char   *extension,
	AssLoaderFn   loader,
	AssReleaseFn  releaser
)
{
    if (!assman || !extension || !loader || !releaser) return;
	if (extension[0] == '.') extension++;

	AssType *existing = AssMan__findFiletype(assman, extension);
	if (existing) {
		existing->Load    = loader;
		existing->Release = releaser;
		return;
	}
	
	size_t bucket = hash_extension(extension);
	
	AssType *node                    = malloc(sizeof(AssType));
	if (!node) return;
	
	node->extension                  = strdup_local(extension);
	if (!node->extension) {
		free(node);
		return;
	}
	node->Load                       = loader;
	node->Release                    = releaser;
	node->next                       = assman->filetype_buckets[bucket];
	assman->filetype_buckets[bucket] = node;
}

void *
AssMan_load(
	AssMan       *assman,
	const char   *path, 
	const char   *key,
	void         *load_data, 
	void         *release_data
)
{
	const char *lookup_key = key? key : path;
	
	const char *ext = strrchr(path, '.'); 
	if (!ext) {
		return NULL;
	}
	ext++;

	AssType *ass_type = AssMan__findFiletype(assman, ext);
	if (!ass_type) {
		return NULL;
	}
	
	if (!assman->root) {
		Asset *asset     = Asset_new(
				ass_type->Load(path, load_data),
				ass_type->Release,
				release_data
			);
		assman->root = AssNode_new(lookup_key, asset);
		return asset->data;
	}

	AssNode *node = AssNode_walk(assman->root, lookup_key);
	if (node && node->asset) {
		/* Already loaded; increment ref_count */
		node->asset->ref_count++;
		return node->asset->data;
	}

	/* Node does not exist -- create new asset */
	Asset *asset = Asset_new(
			ass_type->Load(path, load_data),
			ass_type->Release,
			release_data
		);
	AssNode_insert(&assman->root, lookup_key, asset);
	return asset->data;
}

void
AssMan_release(AssMan *assman, const char *key)
{
	AssType *ass_type = AssMan__findFiletype(assman, key);
	if (!ass_type) return;
	
	AssNode *node = AssNode_walk(assman->root, key);

	if (!node) return;
	if (!node->asset) return;

	if (1 < node->asset->ref_count) {
		node->asset->ref_count--;
		return;
	}

	node->asset->Release(node->asset->data, node->asset->release_data);
	Asset_free(node->asset);
	node->asset = NULL;

	if (node->child) return;
	
	AssNode_unlink(node, assman);
	free(node);
}

void
AssMan_clear(AssMan *assman)
{
	AssMan_clearAssets(assman);
	AssMan_clearRegistry(assman);
}

void
AssMan_clearAssets(AssMan *assman)
{
	AssNode_free(assman->root);
	assman->root       = NULL;
}

void
AssMan_clearRegistry(AssMan *assman)
{
	for (size_t i = 0; i < BUCKET_COUNT; i++) {
		AssType *ass_type = assman->filetype_buckets[i];
		while(ass_type) {
			AssType *next = ass_type->next;
			free(ass_type->extension);
			free(ass_type);
			ass_type = next;
		}
		assman->filetype_buckets[i] = NULL;
	}
}
