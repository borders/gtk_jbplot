/**
 * main.c
 */

#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>

#include "../jbplot.h"

#define NUM_PTS_PER_STEP 500
#define MIN_R 2
#define MAX_R 4
#define NUM_STEPS 300

static trace_handle t1;
GtkWidget *plot;

double r_min = MIN_R;
double r_max = MAX_R;
double y_0 = 0.5;
int zoom_all = 1;

void init_trace_with_data(trace_handle th) {
	int i, j;
	double r;
	float y_min, y_max;
	jbplot_get_y_axis_range((jbplot *)plot, &y_min, &y_max);
	printf("y_min = %g, max = %g\n", y_min, y_max);
	for(r=r_min; r<r_max; r+=(r_max-r_min)/NUM_STEPS) {
		double y = 0.5;
		for(i=0; i < 300; i++) {
			y = r*y*(1-y);
			//y = r*sin(y);
		}
		i = 0;
		j = 0;
		while(i < NUM_PTS_PER_STEP) {
			y = r*y*(1-y);
			if(zoom_all) {
				jbplot_trace_add_point(th, r, y);
				i++;
			}
			else {
				if(y <= y_max && y >= y_min) {
					i++;
					jbplot_trace_add_point(th, r, y);
					j = 0;
				}
				else {
					j++;
				}
				if(j>500) {
					break;
				}
			}

		}

	}
	if(zoom_all) {
		zoom_all = 0;
	}
	printf("done updating...\n");
	return;
}

void zoom_in_cb(jbplot *plot, gfloat xmin, gfloat xmax, gfloat ymin, gfloat ymax, gpointer data) {
	printf("Zoomed In!\n");
	printf("x-range: (%g, %g)\n", xmin, xmax);
	printf("y-range: (%g, %g)\n", ymin, ymax);
	jbplot_trace_clear_data(t1);
	r_min = xmin;
	r_max = xmax;
	y_0 = 0.5*(ymin+ymax);
	init_trace_with_data(t1);
	jbplot_refresh(plot);
	return;
}

void zoom_all_cb(jbplot *plot, gpointer data) {
	printf("Zoom All!\n");
	zoom_all = 1;
	jbplot_trace_clear_data(t1);
	r_min = MIN_R;
	r_max = MAX_R;
	y_0 = 0.5;
	init_trace_with_data(t1);
	jbplot_refresh(plot);
	return;
}


int main (int argc, char **argv) {
	GtkWidget *window;
	GtkWidget *v_box;

	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	v_box = gtk_vbox_new(FALSE, 10);
	gtk_container_add (GTK_CONTAINER (window), v_box);
	
	plot = jbplot_new ();
	g_signal_connect(plot, "zoom-in", G_CALLBACK (zoom_in_cb), NULL);
	g_signal_connect(plot, "zoom-all", G_CALLBACK (zoom_all_cb), NULL);
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

	t1 = jbplot_create_trace((int)(NUM_STEPS*NUM_PTS_PER_STEP));
	printf("trace capacity: %d\n", (int)(NUM_STEPS*NUM_PTS_PER_STEP));
	if(t1==NULL) {
		printf("error creating trace!\n");
		return 0;
	}
	rgb_color_t color = {0.0, 1.0, 0.0};
	jbplot_trace_set_line_props(t1, LINETYPE_NONE, 1.0, &color);
	color.red = 1.0; color.green = 0.0;	color.blue = 0.0;
	//jbplot_trace_set_marker_props(t1, MARKER_CIRCLE, 2.0, &color);
	jbplot_trace_set_marker_props(t1, MARKER_POINT, 2.0, &color);
	init_trace_with_data(t1);
	jbplot_add_trace((jbplot *)plot, t1);

	gtk_main ();

	return 0;
}
