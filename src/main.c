#include <locale.h>
#include "tracker-miner-media.h"

int
main (int   argc,
      char *argv[])
{
	TrackerMiner *decorator;
	GMainLoop *main_loop;
	GError *error = NULL;

	setlocale (LC_ALL, "");

	main_loop = g_main_loop_new (NULL, FALSE);
	decorator = tmm_decorator_new ();

	if (!g_initable_init (G_INITABLE (decorator), NULL, &error)) {
		g_critical ("Could not start miner: %s\n", error->message);
		g_error_free (error);
		return EXIT_FAILURE;
	}

	tracker_miner_start (decorator);
	g_main_loop_run (main_loop);

	tracker_miner_stop (decorator);
	g_main_loop_unref (main_loop);
	g_object_unref (decorator);

	return EXIT_SUCCESS;
}
