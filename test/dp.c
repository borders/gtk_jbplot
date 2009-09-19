/**
 * main.c
 */

#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>

#include "../jbplot.h"

#define m1 1.0
#define m2 1.0
#define L1 1.0
#define L2 1.0
#define g 9.81
#define DT 0.002

static int t = 0;
static trace_handle t1, t2;
GtkWidget *plot;
GtkWidget *canvas;
static int run = 1;


// Global pendulum state variables
static double q1 = 1.0;
static double q2 = 0.0;
static double q1d = 0.0;
static double q2d = 0.0;



void calc_accels(double q1, 
             double q2, 
             double q1d, 
             double q2d, 
             double *theta_1_dd, 
             double *theta_2_dd)
{
double q1dd, q2dd;
double cq1 = cos(q1);
double sq1 = sin(q1);
double cq2 = cos(q2);
double sq2 = sin(q2);
double cq1_2 = cq1 * cq1;
double sq1_2 = sq1 * sq1;
double cq2_2 = cq2 * cq2;
double sq2_2 = sq2 * sq2;

q1dd = -1/L1/(-2*cq1*cq2*sq1*m2*sq2+cq1*cq1*m1*cq2*cq2+cq1*cq1*sq2*sq2*m1+cq1*cq1*sq2*sq2*m2+m1*sq1*sq1*cq2*cq2+m1*sq1*sq1*sq2*sq2+cq2*cq2*m2*sq1*sq1)*(cq2*cq2*m2*sq1*L1*q1d*q1d*cq1+cq2*cq2*m2*sq1*g-cq2*m2*cq1*cq1*sq2*L1*q1d*q1d-cq2*m2*cq1*sq2*g+cq2*sq1*sq1*L1*q1d*q1d*m2*sq2+cq2*cq2*cq2*sq1*L2*q2d*q2d*m2+cq2*sq1*L2*q2d*q2d*sq2*sq2*m2-cq1*sq2*sq2*L1*q1d*q1d*sq1*m2-cq1*sq2*L2*q2d*q2d*m2*cq2*cq2-cq1*sq2*sq2*sq2*L2*q2d*q2d*m2+m1*sq1*g*cq2*cq2+m1*sq1*g*sq2*sq2);

q2dd = (cos(q1)*sin(q1)*L2*q2d*q2d*m2*cq2_2+cos(q2)*cos(q1)*m2*sin(q1)*g+cos(q2)*cos(q1)*m1*sin(q1)*g+cos(q2)*cq1_2*m2*sin(q1)*L1*q1d*q1d+cos(q2)*sq1_2*sin(q2)*L2*q2d*q2d*m2+cos(q2)*L1*q1d*q1d*sin(q1)*cq1_2*m1-cos(q2)*cq1_2*sin(q2)*L2*q2d*q2d*m2+cos(q2)*L1*q1d*q1d*sq1_2*sq1*m1+cos(q2)*L1*q1d*q1d*sq1_2*sin(q1)*m2-g*sin(q2)*cq1_2*m1-L1*q1d*q1d*cos(q1)*sin(q2)*m1*sq1_2-cos(q1)*sq1_2*L1*q1d*q1d*m2*sin(q2)-L1*q1d*q1d*cq1_2*cos(q1)*sin(q2)*m1-cq1_2*cos(q1)*m2*sin(q2)*L1*q1d*q1d-cq1_2*m2*sin(q2)*g-cos(q1)*sin(q1)*L2*q2d*q2d*m2*sq2_2)/(-2*cos(q1)*cos(q2)*sin(q1)*m2*sin(q2)+cq1_2*m1*cq2_2+cq1_2*sq2_2*m1+cq1_2*sq2_2*m2+m1*sq1_2*cq2_2+m1*sq1_2*sq2_2+cq2_2*m2*sq1_2)/L2;


*theta_1_dd = q1dd;
*theta_2_dd = q2dd;
return;
}


void time_step(double q1_old, 
             double q2_old, 
             double q1d_old, 
             double q2d_old, 
             double dt,
             double *q1_new,
             double *q2_new,
             double *q1d_new,
             double *q2d_new)
{
	double q1dd, q2dd;
	calc_accels(q1_old, q2_old, q1d_old, q2d_old, &q1dd, &q2dd);
	*q1d_new = q1d_old + q1dd * dt;
	*q2d_new = q2d_old + q2dd * dt;

	*q1_new = q1_old + (*q1d_new)*dt;
	*q2_new = q2_old + (*q2d_new)*dt;
	return;
}



void init_trace_with_data(trace_handle th) {
	for(t=0; t < 10; t++) {
		jbplot_trace_add_point(th, t, 2*sin(0.1*t)+4*sin(0.11*t));
	}
	return;
}

void init_trace_with_data_2(trace_handle th) {
	for(t=0; t < 10; t++) {
		jbplot_trace_add_point(th, t, 2*sin(0.11*t)+4*sin(0.09*t));
	}
	return;
}

gboolean update_data(gpointer data) {
/*
 	static double q1 = 1.0;
	static double q2 = 0.0;
	static double q1d = 0.0;
	static double q2d = 0.0;
*/
	int i;
	if(!run) {
		return TRUE;
	}
	for(i=1; i <= 20; i++) {
		time_step(q1, q2, q1d, q2d, DT, &q1, &q2, &q1d, &q2d);
		//jbplot_trace_add_point(t1, t+i, q1); 
		//jbplot_trace_add_point(t2, t+i, q2); 
	}
	t += 20;
	jbplot_trace_add_point(t1, t, q1); 
	jbplot_trace_add_point(t2, t, q2); 
	gtk_widget_queue_draw(plot);	
	gtk_widget_queue_draw(canvas);	
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



gboolean draw_pendulum (GtkWidget *widget, GdkEventExpose *event, gpointer data) {
  cairo_t *cr = gdk_cairo_create(widget->window);
  int w, h, i;
  gtk_widget_get_size_request(widget, &w, &h);

  // fill background first
  cairo_set_source_rgb (cr, 1, 1, 1); /* white */
  cairo_paint(cr);
 
  cairo_translate(cr, w/2, h/2);
  //cairo_scale(cr, w/(2.*(L1+L2)), -h/(2.*(L1+L2)));
  cairo_scale(cr, 1, -1);

	//draw the upper link
	cairo_set_line_width(cr, 2.0);
	cairo_set_source_rgb(cr, 0, 0, 1.0);
	cairo_move_to(cr, 0.0, 0.0);
	double x1 = L1*sin(q1);
	double y1 = -L1*cos(q1);
	double x2 = x1 + L2*sin(q2);
	double y2 = y1 - L2*cos(q2);
	cairo_line_to(cr, x1 * w/(2.*(L1+L2)), y1 * h/(2.*(L1+L2)));
	cairo_stroke(cr);

	//draw the lower link
	cairo_set_line_width(cr, 2.0);
	cairo_set_source_rgb(cr, 1.0, 0, 0);
	cairo_move_to(cr, x1 * w/(2.*(L1+L2)), y1 * h/(2.*(L1+L2)));
	cairo_line_to(cr, x2 * w/(2.*(L1+L2)), y2 * h/(2.*(L1+L2)));
	cairo_stroke(cr);

	// draw the upper mass
	cairo_set_source_rgb(cr, 0, 0, 1.0);
	cairo_arc(cr, x1 * w/(2.*(L1+L2)), y1 * h/(2.*(L1+L2)), 6.0, 0, 2*M_PI);
	cairo_fill(cr);

	// draw the lower mass
	cairo_set_source_rgb(cr, 1.0, 0, 0.0);
	cairo_arc(cr, x2 * w/(2.*(L1+L2)), y2 * h/(2.*(L1+L2)), 6.0, 0, 2*M_PI);
	cairo_fill(cr);

  cairo_destroy(cr);

  return TRUE;
}


int main (int argc, char **argv) {
	GtkWidget *window;
	GtkWidget *v_box;
	GtkWidget *button;

	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	v_box = gtk_vbox_new(FALSE, 10);
	gtk_container_add (GTK_CONTAINER (window), v_box);
	
	plot = jbplot_new ();
	gtk_widget_set_size_request(plot, 700, 400);
	gtk_box_pack_start (GTK_BOX(v_box), plot, TRUE, TRUE, 0);

	button = gtk_button_new_with_label("Pause");
	gtk_box_pack_start (GTK_BOX(v_box), button, FALSE, FALSE, 0);
	g_signal_connect(button, "clicked", G_CALLBACK(button_activate), NULL);

	canvas = gtk_drawing_area_new();
	gtk_widget_set_size_request(canvas, 700,400);
	gtk_box_pack_start (GTK_BOX(v_box), canvas, TRUE, TRUE, 0);
	g_signal_connect(canvas, "expose_event", G_CALLBACK(draw_pendulum), NULL);

	g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

	gtk_widget_show_all (window);

	g_timeout_add(20, update_data, t1);

	jbplot_set_plot_title((jbplot *)plot, "Hello World", 1);
	jbplot_set_plot_title_visible((jbplot *)plot, 1);
	jbplot_set_x_axis_label((jbplot *)plot, "Time (sec)", 1);
	jbplot_set_x_axis_label_visible((jbplot *)plot, 1);
	jbplot_set_y_axis_label((jbplot *)plot, "Amplitude", 1);
	jbplot_set_y_axis_label_visible((jbplot *)plot, 1);

	rgb_color_t gridline_color = {0.7, 0.7, 0.7};
	jbplot_set_x_axis_gridline_props((jbplot *)plot, LINETYPE_DASHED, 1.0, gridline_color);
	jbplot_set_y_axis_gridline_props((jbplot *)plot, LINETYPE_DASHED, 1.0, gridline_color);

	t1 = jbplot_create_trace(8000);
	t2 = jbplot_create_trace(8000);
	if(t1==NULL || t2==NULL) {
		printf("error creating traces!\n");
		return 0;
	}
	rgb_color_t color = {0.0, 1.0, 0.0};
	jbplot_trace_set_line_props(t1, LINETYPE_SOLID, 2.0, color);
	color.red = 1.0; color.green = 0.0;	color.blue = 0.0;
	jbplot_trace_set_line_props(t2, LINETYPE_SOLID, 2.0, color);
	jbplot_trace_set_name(t1, "theta_1");
	jbplot_trace_set_name(t2, "theta_2");
	//init_trace_with_data(t1);
	//init_trace_with_data_2(t2);
	jbplot_add_trace((jbplot *)plot, t1);
	jbplot_add_trace((jbplot *)plot, t2);

	gtk_main ();

	return 0;
}
