/**
 * main.c
 */

#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>

#include "../jbplot.h"


static trace_handle t1;
GtkWidget *plot;


float x[] = {1,2,3,4,5,6,7,8,9};
float y[] = {1,3,2,4,5,7,6,8,9};

int main (int argc, char **argv) {
	GtkWidget *window;
	GtkWidget *v_box;

	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	v_box = gtk_vbox_new(FALSE, 10);
	gtk_container_add (GTK_CONTAINER (window), v_box);
	
	plot = jbplot_new ();
	gtk_widget_set_size_request(plot, 700, 700);
	gtk_box_pack_start (GTK_BOX(v_box), plot, TRUE, TRUE, 0);

	g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

	gtk_widget_show_all (window);


	jbplot_set_plot_title((jbplot *)plot, "Hello World", 1);
	jbplot_set_plot_title_visible((jbplot *)plot, 1);
	jbplot_set_x_axis_label((jbplot *)plot, "r", 1);
	jbplot_set_x_axis_label_visible((jbplot *)plot, 1);
	jbplot_set_y_axis_label((jbplot *)plot, "f(x) = r x (1-x)", 1);
	jbplot_set_y_axis_label_visible((jbplot *)plot, 1);

	rgb_color_t gridline_color = {0.7, 0.7, 0.7};
	jbplot_set_x_axis_gridline_props((jbplot *)plot, LINETYPE_DASHED, 1.0, &gridline_color);
	jbplot_set_y_axis_gridline_props((jbplot *)plot, LINETYPE_DASHED, 1.0, &gridline_color);

	t1 = jbplot_create_trace(1);
	if(t1==NULL) {
		printf("error creating trace!\n");
		return 0;
	}
	rgb_color_t color = {0.0, 1.0, 0.0};
	jbplot_trace_set_line_props(t1, LINETYPE_SOLID, 1.0, &color);
	color.red = 1.0; color.green = 0.0;	color.blue = 0.0;
	jbplot_trace_set_marker_props(t1, MARKER_POINT, 2.0, &color);
	jbplot_add_trace((jbplot *)plot, t1);
	jbplot_trace_set_data(t1, x, y, 9);

	gtk_main ();

	return 0;
}
