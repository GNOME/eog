#include <config.h>
#include "gpi-mgr.h"

#include <glib/gmem.h>

#include <gconf/gconf-client.h>

#define _(s) (s)

enum {
	SETTINGS_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

enum {
	PROP_0,
	PROP_NAME
};

struct _GPIMgrPrivate {
	gchar *name;
};

static GObjectClass *parent_class = NULL;

static void
gpi_mgr_finalize (GObject *object)
{
	GPIMgr *mgr = GPI_MGR (object);

	g_array_free (mgr->selection, TRUE);
	g_free (mgr->priv->name);
	g_free (mgr->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gpi_mgr_set_property (GObject *o, guint n, const GValue *v, GParamSpec *pspec)
{
	GPIMgr *mgr = GPI_MGR (o);

	switch (n) {
	case PROP_NAME:
		g_free (mgr->priv->name);
		mgr->priv->name = g_strdup (g_value_peek_pointer (v));
		break;
	default:
		break;
	}
}

static void
gpi_mgr_get_property (GObject *o, guint n, GValue *v, GParamSpec *pspec)
{
	GPIMgr *mgr = GPI_MGR (o);

	switch (n) {
	case PROP_NAME:
		g_value_set_string (v, mgr->priv->name);
		break;
	}
}

static void
gpi_mgr_class_init (gpointer klass, gpointer class_data)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->finalize = gpi_mgr_finalize;
	gobject_class->set_property = gpi_mgr_set_property;
	gobject_class->get_property = gpi_mgr_get_property;

	g_object_class_install_property (gobject_class, PROP_NAME,
		g_param_spec_string ("name", _("Name"),
			_("The name of the file or data to print"), "",
			G_PARAM_READWRITE));

	parent_class = g_type_class_peek_parent (klass);

	signals[SETTINGS_CHANGED] = g_signal_new ("settings_changed",
		G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (GPIMgrClass, settings_changed),
		NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

static void
gpi_mgr_instance_init (GTypeInstance *instance, gpointer klass)
{
	GPIMgr *mgr = GPI_MGR (instance);

	mgr->selection = g_array_new (TRUE, TRUE, sizeof (guint));
	mgr->priv = g_new0 (GPIMgrPrivate, 1);
}

GType
gpi_mgr_get_type (void)
{
	static GType t = 0;

	if (!t) {
		static const GTypeInfo i = {
			sizeof (GPIMgrClass), NULL, NULL,
			gpi_mgr_class_init, NULL, NULL, sizeof (GPIMgr), 0,
			gpi_mgr_instance_init, NULL
		};

		t = g_type_register_static (G_TYPE_OBJECT, "GPIMgr", &i,
					    G_TYPE_FLAG_ABSTRACT);
	}

	return t;
}

GnomePrintJob *
gpi_mgr_get_job (GPIMgr *mgr)
{
	GPIMgrClass *klass;

	g_return_val_if_fail (GPI_IS_MGR (mgr), NULL);

	klass = G_TYPE_INSTANCE_GET_CLASS (mgr, GPI_TYPE_MGR, GPIMgrClass);
	return klass->get_job ? klass->get_job (mgr) : NULL;
}

guint
gpi_mgr_get_n_pages (GPIMgr *mgr)
{
	GPIMgrClass *klass;

	g_return_val_if_fail (GPI_IS_MGR (mgr), 0);

	klass = G_TYPE_INSTANCE_GET_CLASS (mgr, GPI_TYPE_MGR, GPIMgrClass);
	return klass->get_n_pages ? klass->get_n_pages (mgr) : 0;
}

void
gpi_mgr_dump_settings (GPIMgr *mgr)
{
	g_return_if_fail (GPI_IS_MGR (mgr));

	g_message ("Margins: %.2f, %.2f, %.2f, %.2f",
		   mgr->settings.margin.left, mgr->settings.margin.right,
		   mgr->settings.margin.top, mgr->settings.margin.bottom);
	g_message ("Page: %.2f x %.2f", mgr->settings.page.width,
		   mgr->settings.page.height);
}

void
gpi_mgr_load_settings (GPIMgr *mgr)
{
	GConfClient *c;
	GPIMgrClass *klass;

	g_return_if_fail (GPI_IS_MGR (mgr));

	c = gconf_client_get_default ();
	mgr->settings.margin.top = gconf_client_get_float (c,
		"/apps/eog/viewer/print_settings/margin_top", NULL);
	mgr->settings.margin.bottom = gconf_client_get_float (c,
		"/apps/eog/viewer/print_settings/margin_bottom", NULL);
	mgr->settings.margin.right = gconf_client_get_float (c,
		"/apps/eog/viewer/print_settings/margin_right", NULL); 
	mgr->settings.margin.left = gconf_client_get_float (c,
		"/apps/eog/viewer/print_settings/margin_left", NULL);
	mgr->settings.margin.print = gconf_client_get_bool (c,
		"/apps/eog/viewer/print_settings/margin_print", NULL);
	mgr->settings.page.width = gconf_client_get_float (c,
		"/apps/eog/viewer/print_settings/page_width", NULL);
	mgr->settings.page.height = gconf_client_get_float (c,
		"/apps/eog/viewer/print_settings/page_height", NULL);
	mgr->settings.orient = gconf_client_get_int (c,
		"/apps/eog/viewer/print_settings/orient", NULL);
	g_object_unref (c);

	klass = G_TYPE_INSTANCE_GET_CLASS (mgr, GPI_TYPE_MGR, GPIMgrClass);
	if (klass->load_settings) klass->load_settings (mgr);

	g_signal_emit (mgr, signals[SETTINGS_CHANGED], 0);
}

void
gpi_mgr_save_settings (GPIMgr *mgr)
{
	GConfClient *c;
	GPIMgrClass *klass;

	g_return_if_fail (GPI_IS_MGR (mgr));

	c = gconf_client_get_default ();
	gconf_client_set_float (c,
		"/apps/eog/viewer/print_settings/margin_top",
		mgr->settings.margin.top, NULL);
	gconf_client_set_float (c,
		"/apps/eog/viewer/print_settings/margin_bottom",
		mgr->settings.margin.bottom, NULL);
	gconf_client_set_float (c,
		"/apps/eog/viewer/print_settings/margin_right",
		mgr->settings.margin.right, NULL);
	gconf_client_set_float (c,
		"/apps/eog/viewer/print_settings/margin_left",
		mgr->settings.margin.left, NULL);
	gconf_client_set_bool (c,
		"/apps/eog/viewer/print_settings/margin_print",
		mgr->settings.margin.print, NULL);
	gconf_client_set_float (c,
		"/apps/eog/viewer/print_settings/page_width",
		mgr->settings.page.width, NULL);
	gconf_client_set_float (c,
		"/apps/eog/viewer/print_settings/page_height",
		mgr->settings.page.height, NULL);
	gconf_client_set_int (c,
		"/apps/eog/viewer/print_settings/orient",
		mgr->settings.orient, NULL);
	g_object_unref (c);

	klass = G_TYPE_INSTANCE_GET_CLASS (mgr, GPI_TYPE_MGR, GPIMgrClass);
	if (klass->save_settings) klass->save_settings (mgr);
}
