#include <glib.h>
#include <eog-image.h>

typedef struct {
	char *name;
	char *path;
	unsigned w;
	unsigned h;
	gboolean result;
	gboolean is_jpeg;
	gboolean is_animation;
} LoadData;

static LoadData load_data[] = {
	{
		.name = "jpeg",		.path = "eog.jpg",	.w = 10,	.h = 10,
		.result = TRUE,		.is_jpeg = TRUE,	.is_animation = FALSE
	},
	{ 
		.name = "png",		.path = "eog.png",	.w = 10,	.h = 10,
		.result = TRUE,		.is_jpeg = FALSE,	.is_animation = FALSE
	},
	{
		.name = "svg",		.path = "eog.svg",	.w = 10,	.h = 10,
		.result = TRUE,		.is_jpeg = FALSE,	.is_animation = FALSE
	},
	{
		.name = "gif",		.path = "eog.gif",	.w = 50,	.h = 50,
		.result = TRUE,		.is_jpeg = FALSE,	.is_animation = TRUE
	},
	{
		.name = "non-existing",	.path = "dummy",
		.result = FALSE,	.is_jpeg = FALSE,	.is_animation = FALSE
	}
};

static void
load (gconstpointer func_data)
{
	LoadData *data = (LoadData *) func_data;
	EogImage *image;
	gboolean result;
	GFile *file;
	char *filename;

	filename = g_test_build_filename (G_TEST_DIST, "files", data->path, NULL);
	file = g_file_new_for_path (filename);
	image = eog_image_new_file (file);
	g_object_unref (file);
	g_free (filename);

	result = eog_image_load (image, EOG_IMAGE_DATA_ALL, NULL, NULL);
	g_assert (result == data->result);

	if (result) {
		GdkPixbuf *pixbuf;

		g_assert (eog_image_is_jpeg (image) == data->is_jpeg);
		g_assert (eog_image_is_animation (image) == data->is_animation);

		pixbuf = eog_image_get_pixbuf (image);
		g_assert (GDK_IS_PIXBUF (pixbuf));

		g_assert_cmpint (gdk_pixbuf_get_width (pixbuf), ==, data->w);
		g_assert_cmpint (gdk_pixbuf_get_height (pixbuf), ==, data->h);

		g_object_unref (pixbuf);
	}

	g_object_unref (image);
}

int
main (int argc, char **argv)
{
	int result;
	unsigned i;

	g_test_init (&argc, &argv, NULL);

	for (i = 0; i < G_N_ELEMENTS (load_data); i++) {
		char *test_path;

		test_path = g_strdup_printf ("/image/load/%s", load_data[i].name);
		g_test_add_data_func (test_path, &load_data[i], load);
		g_free (test_path);
	}

	result = g_test_run ();

	return result;
}
