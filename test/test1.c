/**
 * main.c
 */

#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>

#include "../jbplot.h"

static int t = 0;
static trace_handle th;
GtkWidget *plot;
static int run = 1;

void init_trace_with_data(trace_handle th) {
	for(t=0; t < 10; t++) {
		jbplot_trace_add_point(th, t, 2*sin(0.1*t)+4*sin(0.11*t));
	}
	return;
}

gboolean update_data(gpointer data) {
	int i;
	//printf("hello!\n");
	if(!run) {
		return TRUE;
	}
	for(i=1; i <= 5; i++) {
		jbplot_trace_add_point(th, t+i, 2*sin(0.1*(t+i))+4*sin(0.11*(t+i))); 
	}
	t += 5;
	gtk_widget_queue_draw(plot);	
	return TRUE;
}

void button_activate(GtkButton *b, gpointer data) {
	run = !run;
	if(run) {
		gtk_button_set_label(b, "Pause");
	}
	else {
		gtk_button_set_label(b, "Resume");
	}
	//printf("button activated!\n");
	return;
}

int main (int argc, char **argv) {
	GtkWidget *window;
	GtkWidget *v_box;
	GtkWidget *button;
	//trace_handle th;

	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	v_box = gtk_vbox_new(FALSE, 10);
	gtk_container_add (GTK_CONTAINER (window), v_box);
	
	plot = jbplot_new ();
	gtk_widget_set_size_request(plot, 700, 700);
	gtk_box_pack_start (GTK_BOX(v_box), plot, TRUE, TRUE, 0);

	button = gtk_button_new_with_label("Pause");
	gtk_box_pack_start (GTK_BOX(v_box), button, FALSE, FALSE, 0);
	g_signal_connect(button, "clicked", G_CALLBACK(button_activate), NULL);

	g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

	gtk_widget_show_all (window);

	g_timeout_add(50, update_data, th);

	jbplot_set_plot_title((jbplot *)plot, "Hello World", 1);
	jbplot_set_plot_title_visible((jbplot *)plot, 1);
	jbplot_set_x_axis_label((jbplot *)plot, "Time (sec)", 1);
	jbplot_set_x_axis_label_visible((jbplot *)plot, 1);
	jbplot_set_y_axis_label((jbplot *)plot, "Amplitude", 1);
	jbplot_set_y_axis_label_visible((jbplot *)plot, 1);

	rgb_color_t gridline_color = {0.7, 0.7, 0.7};
	jbplot_set_x_axis_gridline_props((jbplot *)plot, LINETYPE_DASHED, 1.0, gridline_color);
	jbplot_set_y_axis_gridline_props((jbplot *)plot, LINETYPE_DASHED, 1.0, gridline_color);

	th = jbplot_create_trace(2000);
	if(th==NULL) {
		printf("error creating trace!\n");
		return 0;
	}
	rgb_color_t color = {0.0, 1.0, 0.0};
	jbplot_trace_set_line_props(th, LINETYPE_SOLID, 2.0, color);
	color.red = 1.0;
	color.green = 0.0;
	color.blue = 0.0;
	jbplot_trace_set_marker_props(th, MARKER_NONE, 5.0, color);
	init_trace_with_data(th);
	jbplot_add_trace((jbplot *)plot, th);

	gtk_main ();

	return 0;
}
