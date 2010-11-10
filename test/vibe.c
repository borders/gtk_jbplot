/**
 * main.c
 */

#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../jbplot.h"

#define m1 1.0
#define m2 1.0
#define L1 1.0
#define L2 1.0
#define g 9.81
#define DT 0.002

static double t = 0.0;
static double h = 1e-3;
static trace_handle t1, t2;
GtkWidget *plot;
GtkWidget *canvas;
static int run = 1;

// Global variables
static FILE *fp;
static int got_eof = 0;
static double ti, y_1, theta;
static double squash_factor = 1.0;

gboolean update_data(gpointer data) {
	int i;
	if(!run) {
		return TRUE;
	}

	// read line of file
	int ret = fscanf(fp, "%lf %lf %lf\n", &ti, &y_1, &theta);
	if(ret==3) {
		printf("Got data: t = %g, y_1 = %g, theta = %g\n", ti, y_1, theta);
		jbplot_trace_add_point(t1, ti, y_1); 
		jbplot_trace_add_point(t2, ti, theta); 
		jbplot_refresh((jbplot *)plot);
		gtk_widget_queue_draw(canvas);	
	}
	else if(ret==EOF) {
		printf("Got to end of file. Done.\n");
		return FALSE;
	}
	else {
		printf("Error parsing file...\n");
	}
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

gboolean draw_it (GtkWidget *widget, GdkEventExpose *event, gpointer data) {
  cairo_t *cr = gdk_cairo_create(widget->window);
  int i;
  double w, h, max_dim, min_dim;
  w = widget->allocation.width;
  h = widget->allocation.height;
	if(w > h) {
		max_dim = w;
		min_dim = h;
	}
	else {
		max_dim = h;
		min_dim = w;
	}
	double scale_factor = min_dim/(2.*(L1+L2));

  // fill background first
  cairo_set_source_rgb (cr, 1, 1, 1); /* white */
  cairo_paint(cr);
 
  cairo_translate(cr, w/2, h/2);
  //cairo_scale(cr, scale_factor, -scale_factor);
  cairo_scale(cr, 1, -1);

	//draw the big mass
#define M_width 100.0
#define M_height 70.0
	double M_y = y_1 * squash_factor;
	cairo_set_line_width(cr, 1.0);
	cairo_set_source_rgb(cr, 0, 0, 1.0);
	cairo_move_to(cr, -M_width/2, M_y + M_height/2);
	cairo_line_to(cr, +M_width/2, M_y + M_height/2);
	cairo_line_to(cr, +M_width/2, M_y - M_height/2);
	cairo_line_to(cr, -M_width/2, M_y - M_height/2);
	cairo_line_to(cr, -M_width/2, M_y + M_height/2);
	cairo_stroke(cr);

	// draw the little (rotating) mass
#define m_L 30
#define m_R 8
	cairo_set_line_width(cr, 0.2*m_L);
	cairo_set_source_rgb(cr, 1, 0, 0);
	cairo_move_to(cr, 0, M_y);
	cairo_line_to(cr, m_L * sin(theta), M_y - m_L*cos(theta));
	cairo_stroke(cr);
	cairo_arc(cr, m_L*sin(theta), M_y - m_L*cos(theta), m_R, 0, 2*M_PI);
	cairo_fill(cr);

  cairo_destroy(cr);

  return TRUE;
}


int main (int argc, char **argv) {
	GtkWidget *window;
	GtkWidget *v_box;
	GtkWidget *button;
	double start_dt;

	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	v_box = gtk_vbox_new(FALSE, 10);
	gtk_container_add (GTK_CONTAINER (window), v_box);
	
	plot = jbplot_new ();
	gtk_widget_set_size_request(plot, 700, 400);
	jbplot_set_antialias((jbplot *)plot, 0);
	gtk_box_pack_start (GTK_BOX(v_box), plot, TRUE, TRUE, 0);

	button = gtk_button_new_with_label("Pause");
	gtk_box_pack_start (GTK_BOX(v_box), button, FALSE, FALSE, 0);
	g_signal_connect(button, "clicked", G_CALLBACK(button_activate), NULL);

	if(argc < 4) {
		printf("Usage: vibe data_file dt_ms vertical_squash_factor\n");
		exit(1);
	}

	char *fname = argv[1];
	if(!strcmp(fname, "-")) {
		fp = stdin;
	}
	else {
		fp = fopen(fname,"r");
		if(!fp) {
			printf("Error opening file: %s\n", fname);
			exit(1);
		}
	}
		
	int dt = atof(argv[2]);
	printf("Using dt = %d\n", dt);

	squash_factor = atof(argv[3]);
	printf("Using squash_factor = %g\n", squash_factor);

	canvas = gtk_drawing_area_new();
	gtk_widget_set_size_request(canvas, 700,400);
	gtk_box_pack_start (GTK_BOX(v_box), canvas, TRUE, TRUE, 0);
	g_signal_connect(canvas, "expose_event", G_CALLBACK(draw_it), NULL);

	g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

	gtk_widget_show_all (window);

	g_timeout_add(dt, update_data, t1);

	jbplot_set_plot_title((jbplot *)plot, "Vibe", 1);
	jbplot_set_plot_title_visible((jbplot *)plot, 1);
	jbplot_set_x_axis_label((jbplot *)plot, "Time (sec)", 1);
	jbplot_set_x_axis_label_visible((jbplot *)plot, 1);
	jbplot_set_y_axis_label((jbplot *)plot, "Amplitude", 1);
	jbplot_set_y_axis_label_visible((jbplot *)plot, 1);

	rgb_color_t gridline_color = {0.7, 0.7, 0.7};
	jbplot_set_x_axis_gridline_props((jbplot *)plot, LINETYPE_DASHED, 1.0, &gridline_color);
	jbplot_set_y_axis_gridline_props((jbplot *)plot, LINETYPE_DASHED, 1.0, &gridline_color);
	jbplot_set_legend_props((jbplot *)plot, 1, NULL, NULL, LEGEND_POS_RIGHT);

	t1 = jbplot_create_trace(3000);
	t2 = jbplot_create_trace(3000);
	if(t1==NULL || t2==NULL) {
		printf("error creating traces!\n");
		return 0;
	}
	rgb_color_t color = {0.0, 1.0, 0.0};
	jbplot_trace_set_line_props(t1, LINETYPE_SOLID, 2.0, &color);
	color.red = 1.0; color.green = 0.0;	color.blue = 0.0;
	jbplot_trace_set_line_props(t2, LINETYPE_SOLID, 2.0, &color);
	jbplot_trace_set_name(t1, "y_1");
	jbplot_trace_set_name(t2, "theta");
	jbplot_add_trace((jbplot *)plot, t1);
	jbplot_add_trace((jbplot *)plot, t2);

	gtk_main ();

	return 0;
}

