/**
 * main.c
 *
 * Test EggClockFace in a GtkWindow
 *
 * (c) 2005, Davyd Madeley
 *
 * Authors:
 *   Davyd Madeley <davyd@madeley.id.au>
 */

#include <gtk/gtk.h>

#include "jbplot.h"

float x[] = {0,1,2,3,4,5,6,7,8,9};
float y[] = {1,0,3,3,7,5,3,9,8,7};



int
main (int argc, char **argv)
{
	GtkWidget *window;
	GtkWidget *plot;

	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	
	plot = jbplot_new ();
	gtk_container_add (GTK_CONTAINER (window), plot);

	g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_main_quit), NULL);

	gtk_widget_show_all (window);

	jbplot_set_plot_title((jbplot *)plot, "Hello World", 1);
	jbplot_add_trace((jbplot *)plot, x, y, 10, 10);

	gtk_main ();
}
