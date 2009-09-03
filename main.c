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
	GtkWidget *v_box;
	GtkWidget *plot;
	GtkWidget *button;

	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	v_box = gtk_vbox_new(FALSE, 10);
	gtk_container_add (GTK_CONTAINER (window), v_box);
	
	plot = jbplot_new ();
	gtk_widget_set_size_request(plot, 300, 300);
	gtk_box_pack_start (GTK_BOX(v_box), plot, TRUE, TRUE, 0);

	button = gtk_button_new_with_label("Press Me!");
	gtk_box_pack_start (GTK_BOX(v_box), button, FALSE, FALSE, 0);

	g_signal_connect (window, "destroy",
			G_CALLBACK (gtk_main_quit), NULL);

	gtk_widget_show_all (window);

	jbplot_set_plot_title((jbplot *)plot, "Hello World", 1);
	jbplot_add_trace((jbplot *)plot, x, y, 10, 10);

	gtk_main ();
}
