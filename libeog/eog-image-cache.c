#include <glib/glist.h>

#include "eog-image-cache.h"
#include "eog-image-private.h"


/* EogImageCache is a static and gloabl image cache which holds all
 * image objects with complete in-memory image representations
 * (ie. eog_image_load has been finished successfully). Objects in the
 * cache will be constantly free'd if other images get added, which in
 * turn yield to more memory usage than MAX_MEM_USAGE.
 */

static GStaticMutex     cache_mutex   = G_STATIC_MUTEX_INIT;

/* The list of all cached images */
static GList           *loaded_images = NULL;

/* Number of currently cached images */
static int               n_cached_images = 0;

/* FIXME: we should make this configurable */
#define MAX_CACHED_IMAGES    5 
#define CACHE_DEBUG          0

/* eog_image_cache_dump: Just a debug function, which dumps the names
 * of the images currently in the cache.
 */
#if CACHE_DEBUG
static void
eog_image_cache_dump (void)
{
	GList *it;

	g_print ("\tImage cache: ");

	if (loaded_images != NULL) {
		for (it = loaded_images; it != NULL; it = it->next) {
			if (EOG_IS_IMAGE (it->data)) 
				g_print ("%s, ", eog_image_get_caption (EOG_IMAGE (it->data)));	
			else
				g_print ("???, ");
		}
	}

	g_print ("\n");
}
#endif

/* Append the image to the cache. Frees images appropriatly if the
 * memory usage is larger than MAX_MEM_USAGE.
 */
void
eog_image_cache_add (EogImage *image)
{
	g_static_mutex_lock (&cache_mutex);

#if CACHE_DEBUG
	g_print ("EogImageCache.add: %s\n", eog_image_get_caption (image));
#endif

	if (n_cached_images >= MAX_CACHED_IMAGES) {
		EogImage *img;

		img = EOG_IMAGE (loaded_images->data);
		eog_image_free_mem_private (img);
		n_cached_images--;
		
#if CACHE_DEBUG
		g_print ("\trelease: %s\n", eog_image_get_caption (img));
#endif

		loaded_images = g_list_delete_link (loaded_images, loaded_images);
	}

	/* Note: we don't ref the object here. Therefore an image can
	 * remove itself from the cache at destruction time.
	 */
	loaded_images = g_list_append (loaded_images, image);
	n_cached_images++;

#if CACHE_DEBUG
	eog_image_cache_dump ();
#endif

	g_static_mutex_unlock (&cache_mutex);
}

/* Removes the image from the cache, but doesn't free any memory. This
 * is called when the EogImage object is destoryed or
 * eog_image_free_mem has been called.
 */
void
eog_image_cache_remove (EogImage *image)
{
	GList *node;

	g_static_mutex_lock (&cache_mutex);

#if CACHE_DEBUG
	g_print ("Cache remove: %s\n", eog_image_get_caption (image));
#endif

	node = g_list_find (loaded_images, image);
	if (node != NULL) {
		n_cached_images--;
		loaded_images = g_list_delete_link (loaded_images, node);
	}
#if CACHE_DEBUG
	else {
		g_print ("    image not cached\n");
	}

	eog_image_cache_dump ();
#endif

	g_static_mutex_unlock (&cache_mutex);
}


/* Puts the image at the end of the cache again, so that it is the
 * last image which will be removed automatically.
 */
void 
eog_image_cache_reload (EogImage *image)
{
	GList *node;

	g_static_mutex_lock (&cache_mutex);
	
#if CACHE_DEBUG
	g_print ("EogImageCache.reload: %s\n", eog_image_get_caption (image));
#endif

	node = g_list_find (loaded_images, image);

	if (node != NULL && node != g_list_last (loaded_images)) {
		loaded_images = g_list_delete_link (loaded_images, node);
		loaded_images = g_list_append (loaded_images, image);
	}

#if CACHE_DEBUG
	eog_image_cache_dump ();
#endif

	g_static_mutex_unlock (&cache_mutex);

}
