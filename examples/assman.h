#ifndef ASSMAN_H
#define ASSMAN_H


typedef struct AssMan AssMan;

typedef void *(*AssLoaderFn)(const char *path,  void *data);
typedef void  (*AssReleaseFn)(void      *asset, void *data);

AssMan *AssMan_new(void);
void    AssMan_free(AssMan *assman);

void    AssMan_registerFiletype(
		AssMan       *assman,
		const char   *extension,
		AssLoaderFn   loader,    
		AssReleaseFn  releaser
	);
void   *AssMan_load(
		AssMan       *assman, 
		const char   *path, 
		const char   *key,
		void         *load_data, 
		void         *release_data
	);
void    AssMan_release(      AssMan *assman, const char *key);
void    AssMan_clear(        AssMan *assman);
void    AssMan_clearAssets(  AssMan *assman);
void    AssMan_clearRegistry(AssMan *assman);


#endif /* ASSMAN_H */
