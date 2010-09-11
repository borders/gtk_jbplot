/**
 * main.c
 */

#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <gsl/gsl_odeiv.h>
#include <sys/time.h>

#include "../jbplot.h"


#define M 	20.
#define m1 	2.
#define m2 	2.0
#define L 	1.0
#define l1 	0.5
#define l2 	0.5
#define g		9.81

static double dt;
static double x[6];
static double x_dot[6];
static double t;
static struct timeval t0;

static trace_handle t1, t2, t3;
GtkWidget *plot;
GtkWidget *canvas;
static int name_changed = 0;


int feval(double *x, double *x_dot) {
	double a11, a12, a13, a21, a22, a31, a33;
	double b1, b2, b3;
	a11 = M + m1 + m2;
	a12 = m1*l1*cos(x[2]);
	a13 = m2*l2*cos(x[4]);
	a21 = cos(x[2]);
	a22 = l1;
	a31 = cos(x[4]);
	a33 = l2;
	b1 = m1*l1*x[3]*x[3]*sin(x[2]) + m2*l2*x[5]*x[5]*sin(x[4]);
	b2 = -g * sin(x[2]);
	b3 = -g * sin(x[4]);

	double D = a11*a22*a33 - a12*a21*a33 - a13*a22*a31;
	double D1 = (a22*a33)*b1 + (-a12*a33)*b2 + (-a13*a22)*b3;
	double D2 = (a21*a33)*b1 + (a11*a33-a13*a31)*b2 + (a13*a21)*b3;
	double D3 = (-a22*a31)*b1 + (a12*a31)*b2 + (a11*a22-a12*a21)*b3;

	if(D/(l1*l2) < M) {
		printf("determinant is too small: (D=%g; M=%g)\n", D, M);
		printf("a11*a22*a33/l1*l2 = %g\n", a11*a22*a33/(l1*l2));
		printf("a12*a21*a33/l1*l2 = %g\n", a12*a21*a33/(l1*l2));
		printf("a13*a22*a31/l1*l2 = %g\n", a13*a22*a31/(l1*l2));
	}

	x_dot[0] = x[1];
	x_dot[1] = D1/D;
	x_dot[2] = x[3];
	x_dot[3] = D2/D;
	x_dot[4] = x[5];
	x_dot[5] = D3/D;

	return 0;
}

int integrate(double *x, double *x_dot, double dt) {
	int i;
	for(i=0; i<6; i++) {
		x[i] = x[i] + x_dot[i] * dt;
	}
	return 0;
}

double time_delta(struct timeval *t2, struct timeval *t1) {
	return (t2->tv_sec-t1->tv_sec + 1.e-6*(t2->tv_usec-t1->tv_usec));
}

gboolean update_data(gpointer data) {
	int i;
	struct timeval t_now;

	gettimeofday(&t_now, NULL);
	while(t < time_delta(&t_now, &t0)) {
		feval(x, x_dot);
		integrate(x, x_dot, dt);
		t += dt;

	}
	jbplot_trace_add_point(t1, t, x[2]); 
	jbplot_trace_add_point(t2, t, x[4]); 
	jbplot_trace_add_point(t3, t, x[0]); 

	jbplot_refresh((jbplot *)plot);
	gtk_widget_queue_draw(canvas);	
	return TRUE;
}

gboolean draw_pendulum (GtkWidget *widget, GdkEventExpose *event, gpointer data) {
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

	double scale_factor = min_dim/(2*(l1+l2+L));

  // fill background first
  cairo_set_source_rgb (cr, 1, 1, 1); /* white */
  cairo_paint(cr);
  cairo_translate(cr, w/2, h/2);
  //cairo_scale(cr, 1, -1);
  cairo_scale(cr, scale_factor, -scale_factor);

	//draw the bar
	cairo_set_line_width(cr, 0.05 * L);
	cairo_set_source_rgb(cr, 0, 0, 1.0);
	cairo_move_to(cr, x[0]-L/2.0, 0.0);
	cairo_line_to(cr, x[0]+L/2.0, 0.0);
	cairo_stroke(cr);

	//draw the left pendulum
	cairo_set_line_width(cr, 0.05 * l1);
	cairo_set_source_rgb(cr, 0, 1.0, 0.0);
	cairo_move_to(cr, x[0]-L/2.0, 0.0);
	cairo_line_to(cr, x[0]-L/2.0+l1*sin(x[2]), -l1*cos(x[2]));
	cairo_stroke(cr);
	cairo_arc(cr, x[0]-L/2.0+l1*sin(x[2]),-l1*cos(x[2]), 0.2*l1, 0, 2*M_PI);
	cairo_fill(cr);

	//draw the right pendulum
	cairo_set_line_width(cr, 0.05 * l1);
	cairo_set_source_rgb(cr, 1, 0, 0);
	cairo_move_to(cr, x[0]+L/2.0, 0.0);
	cairo_line_to(cr, x[0]+L/2.0+l2*sin(x[4]), -l2*cos(x[4]));
	cairo_stroke(cr);
	cairo_arc(cr, x[0]+L/2.0+l2*sin(x[4]),-l2*cos(x[4]), 0.2*l2, 0, 2*M_PI);
	cairo_fill(cr);

  cairo_destroy(cr);

  return TRUE;
}


int main (int argc, char **argv) {

	GtkWidget *window;
	GtkWidget *v_box;

	t = 0.0;
	// init speeds
	x[1] = 0.0;
	x[3] = 0.0;
	x[5] = 0.0;

	// init positions
	x[0] = 0.0;
	x[2] = -1.4;
	x[4] = 1.57;


	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	v_box = gtk_vbox_new(FALSE, 10);
	gtk_container_add (GTK_CONTAINER (window), v_box);
	
	plot = jbplot_new ();
	gtk_widget_set_size_request(plot, 700, 400);
	jbplot_set_antialias((jbplot *)plot, 0);
	gtk_box_pack_start (GTK_BOX(v_box), plot, TRUE, TRUE, 0);

	if(argc > 1) {
		if(sscanf(argv[1], "%lg", &dt) != 1) {
			printf("Error parsing dt parameter\n");
			return -1;
		}
	}



	canvas = gtk_drawing_area_new();
	gtk_widget_set_size_request(canvas, 700,400);
	gtk_box_pack_start (GTK_BOX(v_box), canvas, TRUE, TRUE, 0);
	g_signal_connect(canvas, "expose_event", G_CALLBACK(draw_pendulum), NULL);

	g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

	gtk_widget_show_all (window);

	g_timeout_add(40, update_data, t1);

	jbplot_set_plot_title((jbplot *)plot, "Double Pendulum", 1);
	jbplot_set_plot_title_visible((jbplot *)plot, 1);
	jbplot_set_x_axis_label((jbplot *)plot, "Time (sec)", 1);
	jbplot_set_x_axis_label_visible((jbplot *)plot, 1);
	jbplot_set_y_axis_label((jbplot *)plot, "Amplitude", 1);
	jbplot_set_y_axis_label_visible((jbplot *)plot, 1);

	rgb_color_t gridline_color = {0.7, 0.7, 0.7};
	jbplot_set_x_axis_gridline_props((jbplot *)plot, LINETYPE_DASHED, 1.0, &gridline_color);
	jbplot_set_y_axis_gridline_props((jbplot *)plot, LINETYPE_DASHED, 1.0, &gridline_color);
	jbplot_set_legend_props((jbplot *)plot, 1, NULL, NULL, LEGEND_POS_RIGHT);
	jbplot_legend_refresh((jbplot *)plot);

	t1 = jbplot_create_trace(30000);
	t2 = jbplot_create_trace(30000);
	t3 = jbplot_create_trace(30000);
	if(t1==NULL || t2==NULL || t3==NULL) {
		printf("error creating traces!\n");
		return 0;
	}
	rgb_color_t color = {0.0, 1.0, 0.0};
	jbplot_trace_set_line_props(t1, LINETYPE_SOLID, 2.0, &color);
	color.red = 1.0; color.green = 0.0;	color.blue = 0.0;
	jbplot_trace_set_line_props(t2, LINETYPE_SOLID, 2.0, &color);
	color.red = 0.0; color.green = 0.0;	color.blue = 1.0;
	jbplot_trace_set_line_props(t3, LINETYPE_SOLID, 2.0, &color);
	jbplot_trace_set_name(t1, "theta_1");
	jbplot_trace_set_name(t2, "theta_2");
	jbplot_trace_set_name(t3, "x_bar");
	jbplot_add_trace((jbplot *)plot, t1);
	jbplot_add_trace((jbplot *)plot, t2);
	jbplot_add_trace((jbplot *)plot, t3);

	gettimeofday(&t0, NULL);
	gtk_main ();

	return 0;
}


