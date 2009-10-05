/**
 * main.c
 */

#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <gsl/gsl_odeiv.h>

#include "../jbplot.h"

#define NUM_MASSES_PER_BALL 10
#define BALL_MASS 1.0
#define ALPHA 100000.

#define BALL_1_INIT_POS -0.1
#define BALL_1_INIT_VEL 10.0

static double t = 0.0;
static double h = 1e-3;
static trace_handle pos_traces[3*NUM_MASSES_PER_BALL];
static trace_handle vel_traces[3*NUM_MASSES_PER_BALL];
GtkWidget *pos_plot;
GtkWidget *vel_plot;
GtkWidget *canvas;
GtkWidget *dt_scale;
static int run = 1;
static int name_changed = 0;
int n = 3 * NUM_MASSES_PER_BALL;


int state_func(double t, const double *x, double *xd, void *params);


// Global pendulum state variables
static double x[2*NUM_MASSES_PER_BALL*3];
static double k[2*NUM_MASSES_PER_BALL*3];
gsl_odeiv_system sys = {state_func, NULL, 4, NULL};
gsl_odeiv_step_type *step_type;
gsl_odeiv_step *step;
gsl_odeiv_control *control;
gsl_odeiv_evolve *evolve;


#define POS(X,I) X[2*(I)]
#define VEL(X,I) X[2*(I)+1]


rgb_color_t rgb_scale(float x, float y) {
	rgb_color_t c;
	if(x < 0) x = 0;
	else if(x > 1) x = 1;
	if(y < -1) y = -1;
	else if(y > 1) y = 1;

	if(x < 0.5) {
		c.red = 1 - 2*x + y;
		c.green = 2*x + y;
		c.blue = 0.0 + y;
	}
	else {
		c.red = 0.0 + y;
		c.green = 2 - 2*x + y;
		c.blue = -1 + 2*x + y;
	}

	if(c.red < 0) c.red = 0;
	else if(c.red > 1) c.red = 1;
	if(c.green < 0) c.green = 0;
	else if(c.green > 1) c.green = 1;
	if(c.blue < 0) c.blue = 0;
	else if(c.blue > 1) c.blue = 1;
	//printf("%.2f, %.2f, %.2f\n", c.red,c.green,c.blue);
	return c;
}


int state_func(double t, const double *x, double *xd, void *params) {
	int i;
	// calc spring constants
	for(i=0; i<n-1; i++) {
		if(i==(NUM_MASSES_PER_BALL-1) || i==(2*NUM_MASSES_PER_BALL-1)) {
			if(x[2*(i+1)] < x[2*i]) {
				k[i] = ALPHA * NUM_MASSES_PER_BALL;
			}
			else {
				k[i] = 0;
			}
		}
		else {
			k[i] = ALPHA * NUM_MASSES_PER_BALL;
		}
		//printf("k[%d] = %g, ", i, k[i]);
	}
	//printf("\n");

	xd[0] = VEL(x,0);
	xd[1] = 1./(BALL_MASS/NUM_MASSES_PER_BALL) * k[0] * (POS(x,1)-POS(x,0));
	for(i=1; i<n-1; i++) {
		xd[2*i] = VEL(x,i);
		xd[2*i+1] = 1./(BALL_MASS/NUM_MASSES_PER_BALL) * (k[i-1] * (POS(x,i-1)-POS(x,i)) + k[i] * (POS(x,i+1)-POS(x,i)));
	}
	xd[2*n-2] = VEL(x,n-1);
	xd[2*n-1] = 1./(BALL_MASS/NUM_MASSES_PER_BALL) * -k[n-2] * (POS(x,n-1)-POS(x,n-2));
	return 0;
}


gboolean update_data(gpointer data) {
	int i;
	if(!run) {
		return TRUE;
	}

	gdouble v = 0.0;
	if(dt_scale !=NULL) {
		v = gtk_range_get_value(GTK_RANGE(dt_scale));
	}

	double t_next = t + v;
	while(t < t_next) {	
		gsl_odeiv_evolve_apply(evolve, control, step, &sys, &t, t+v, &h, x);
	}

/*	
	for(i=0; i<n; i++) {
		printf("x[%d] = %g, ", i, POS(x,i));
	}
	printf("\n");
*/
/*	
	for(i=0; i<n; i++) {
		printf("v[%d] = %g, ", i, VEL(x,i));
	}
	printf("\n");
*/


	for(i=0; i<n; i++) {
		jbplot_trace_add_point(pos_traces[i], t, POS(x,i)); 
		jbplot_trace_add_point(vel_traces[i], t, VEL(x,i)); 
	}


	jbplot_refresh((jbplot *)pos_plot);
	jbplot_refresh((jbplot *)vel_plot);
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

void save_button_activate(GtkButton *b, gpointer data) {
	jbplot_capture_png((jbplot *)pos_plot, "capture.png");
	GtkWidget *tab1_label = gtk_label_new("Position");
	//printf("save button activated!\n");
	return;
}


void save_button_activate_2(GtkButton *b, gpointer data) {
	GtkWidget *dialog;
	dialog = gtk_file_chooser_dialog_new("Save file to...",
	                                     (GtkWindow *)pos_plot,
	                                     GTK_FILE_CHOOSER_ACTION_SAVE,
	                                     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	                                     GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
	                                     NULL);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
	if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		char *filename;
		char fname[1000];
		fname[0] = '\0';
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		strncat(fname, filename, 999);
		int fl = strlen(filename);
		if(        filename[fl-4]  != '.' || 
		   tolower(filename[fl-3]) != 's' || 
		   tolower(filename[fl-2]) != 'v' || 
		   tolower(filename[fl-1]) != 'g') 
			{
			printf("Adding SVG extension\n");
			strncat(fname, ".svg", 999-strlen(fname));
		}
		jbplot_capture_svg((jbplot *)pos_plot, fname);
		g_free(filename);
	}
	gtk_widget_destroy(dialog);
	return;
}



gboolean draw_balls(GtkWidget *widget, GdkEventExpose *event, gpointer data) {
  cairo_t *cr = gdk_cairo_create(widget->window);
  int i;
  double w, h, max_dim, min_dim;
  w = widget->allocation.width;
  h = widget->allocation.height;
	double margin = 0.3 * w;
	double mass_spacing = (w - 2*margin)/(n-1);
	double ball_diam = mass_spacing * NUM_MASSES_PER_BALL;
	double fudge = 2000;
	if(w > h) {
		max_dim = w;
		min_dim = h;
	}
	else {
		max_dim = h;
		min_dim = w;
	}

  // fill background first
  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_paint(cr);
 
	double b1_center = margin + ball_diam/2;
	double b2_center = margin + ball_diam/2 + ball_diam;
	double b3_center = margin + ball_diam/2 + 2 * ball_diam;

	cairo_set_line_width(cr, mass_spacing/2);
	for(i=0; i<n; i++) {
		rgb_color_t color;
		if(i<NUM_MASSES_PER_BALL) {
			color = rgb_scale(0.0, -0.6 + 1.2/NUM_MASSES_PER_BALL * i);
			cairo_set_source_rgb(cr, color.red, color.green, color.blue);
			double H = margin + i * mass_spacing + fudge*POS(x,i);
			double V = sqrt(pow(ball_diam/2,2) - pow((i+0.5)*mass_spacing - ball_diam/2,2));
			cairo_move_to(cr, H, h/2 - V);
			cairo_line_to(cr, H, h/2 + V);
			cairo_stroke(cr);
		}
		else if(i<2*NUM_MASSES_PER_BALL) {
			color = rgb_scale(0.5, -0.6 + 1.2/NUM_MASSES_PER_BALL * (i-NUM_MASSES_PER_BALL));
			cairo_set_source_rgb(cr, color.red, color.green, color.blue);
			double H = margin + i * mass_spacing + fudge*POS(x,i);
			double V = sqrt(pow(ball_diam/2,2) - pow((i-NUM_MASSES_PER_BALL+0.5)*mass_spacing - ball_diam/2,2));
			cairo_move_to(cr, H, h/2 - V);
			cairo_line_to(cr, H, h/2 + V);
			cairo_stroke(cr);
		}
		else {
			color = rgb_scale(1.0, -0.6 + 1.2/NUM_MASSES_PER_BALL * (i-2*NUM_MASSES_PER_BALL));
			cairo_set_source_rgb(cr, color.red, color.green, color.blue);
			double H = margin + i * mass_spacing + fudge*POS(x,i);
			double V = sqrt(pow(ball_diam/2,2) - pow((i-2*NUM_MASSES_PER_BALL+0.5)*mass_spacing - ball_diam/2,2));
			cairo_move_to(cr, H, h/2 - V);
			cairo_line_to(cr, H, h/2 + V);
			cairo_stroke(cr);
		}
		cairo_arc(cr, margin + i*mass_spacing + fudge*POS(x,i), h/2, mass_spacing/2, 0, 2*M_PI);
		cairo_fill(cr);
	}

  cairo_destroy(cr);

  return TRUE;
}


int main (int argc, char **argv) {
	GtkWidget *window;
	GtkWidget *v_box;
	GtkWidget *button;
	GtkWidget *plot_notebook;
	GtkWidget *save_button;
	GtkWidget *save_button_2;
	double start_dt;

	step_type = (gsl_odeiv_step_type *)gsl_odeiv_step_rkf45;
	step = gsl_odeiv_step_alloc(step_type, 2*n);
	control = gsl_odeiv_control_y_new(1.e-6, 0.0);
	evolve = gsl_odeiv_evolve_alloc(2*n);
	int i;
	for(i=0; i<n; i++) {
		x[i] = 0.0;
	}
	for(i=0; i<NUM_MASSES_PER_BALL; i++) {
		POS(x,i) = BALL_1_INIT_POS;
		VEL(x,i) = BALL_1_INIT_VEL;
	}

	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	v_box = gtk_vbox_new(FALSE, 10);
	gtk_container_add (GTK_CONTAINER (window), v_box);

	plot_notebook = gtk_notebook_new();
	GtkWidget *tab1_label = gtk_label_new("Position");
	GtkWidget *tab2_label = gtk_label_new("Velocity");
	
	pos_plot = jbplot_new ();
	gtk_widget_set_size_request(pos_plot, 700, 400);
	jbplot_set_antialias((jbplot *)pos_plot, 0);
	gtk_notebook_append_page(GTK_NOTEBOOK(plot_notebook),pos_plot, tab1_label);

	vel_plot = jbplot_new ();
	gtk_widget_set_size_request(vel_plot, 700, 400);
	jbplot_set_antialias((jbplot *)vel_plot, 0);
	gtk_notebook_append_page(GTK_NOTEBOOK(plot_notebook),vel_plot, tab2_label);

	gtk_box_pack_start (GTK_BOX(v_box), plot_notebook, TRUE, TRUE, 0);

	button = gtk_button_new_with_label("Pause");
	gtk_box_pack_start (GTK_BOX(v_box), button, FALSE, FALSE, 0);
	g_signal_connect(button, "clicked", G_CALLBACK(button_activate), NULL);

	save_button = gtk_button_new_with_label("Capture Plot to File (PNG)");
	gtk_box_pack_start (GTK_BOX(v_box), save_button, FALSE, FALSE, 0);
	g_signal_connect(save_button, "clicked", G_CALLBACK(save_button_activate), NULL);

	save_button_2 = gtk_button_new_with_label("Capture Plot to File (SVG)");
	gtk_box_pack_start (GTK_BOX(v_box), save_button_2, FALSE, FALSE, 0);
	g_signal_connect(save_button_2, "clicked", G_CALLBACK(save_button_activate_2), NULL);

	dt_scale = gtk_hscale_new_with_range(0.000025, 0.0005, 0.000025);
	gtk_scale_set_digits((GtkScale *)dt_scale, 5);
	gtk_box_pack_start (GTK_BOX(v_box), dt_scale, FALSE, FALSE, 0);

	if(argc > 1) {
		if(sscanf(argv[1], "%lg", &start_dt) != 1) {
			printf("Error parsing start_dt parameter\n");
			return -1;
		}
		else {
			gtk_range_set_value((GtkRange *)dt_scale, start_dt);
		}
	}


	canvas = gtk_drawing_area_new();
	gtk_widget_set_size_request(canvas, 700,400);
	gtk_box_pack_start (GTK_BOX(v_box), canvas, TRUE, TRUE, 0);
	g_signal_connect(canvas, "expose_event", G_CALLBACK(draw_balls), NULL);

	g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

	gtk_widget_show_all (window);

	g_timeout_add(30, update_data, NULL);

	jbplot_set_plot_title((jbplot *)pos_plot, "Position Plot", 1);
	jbplot_set_plot_title_visible((jbplot *)pos_plot, 1);
	jbplot_set_x_axis_label((jbplot *)pos_plot, "Time (sec)", 1);
	jbplot_set_x_axis_label_visible((jbplot *)pos_plot, 1);
	jbplot_set_y_axis_label((jbplot *)pos_plot, "Amplitude", 1);
	jbplot_set_y_axis_label_visible((jbplot *)pos_plot, 1);

	rgb_color_t gridline_color = {0.7, 0.7, 0.7};
	jbplot_set_x_axis_gridline_props((jbplot *)pos_plot, LINETYPE_DASHED, 1.0, &gridline_color);
	jbplot_set_y_axis_gridline_props((jbplot *)pos_plot, LINETYPE_DASHED, 1.0, &gridline_color);
	jbplot_set_legend_props((jbplot *)pos_plot, 1, NULL, NULL, LEGEND_POS_RIGHT);

	jbplot_set_plot_title((jbplot *)vel_plot, "Velocity Plot", 1);
	jbplot_set_plot_title_visible((jbplot *)vel_plot, 1);
	jbplot_set_x_axis_label((jbplot *)vel_plot, "Time (sec)", 1);
	jbplot_set_x_axis_label_visible((jbplot *)vel_plot, 1);
	jbplot_set_y_axis_label((jbplot *)vel_plot, "Amplitude", 1);
	jbplot_set_y_axis_label_visible((jbplot *)vel_plot, 1);

	jbplot_set_x_axis_gridline_props((jbplot *)vel_plot, LINETYPE_DASHED, 1.0, &gridline_color);
	jbplot_set_y_axis_gridline_props((jbplot *)vel_plot, LINETYPE_DASHED, 1.0, &gridline_color);
	jbplot_set_legend_props((jbplot *)vel_plot, 1, NULL, NULL, LEGEND_POS_RIGHT);

	rgb_color_t color;
	for(i=0; i<n; i++) {
		char trace_name[100];
		vel_traces[i] = jbplot_create_trace(3000);
		pos_traces[i] = jbplot_create_trace(3000);
		if(vel_traces[i] == NULL) {
			printf("error creating trace!\n");
			return 0;
		}
		if(pos_traces[i] == NULL) {
			printf("error creating trace!\n");
			return 0;
		}
		if(i < NUM_MASSES_PER_BALL) {
			color = rgb_scale(0.0, -0.6 + 1.2/NUM_MASSES_PER_BALL * i);
		}
		else if(i < 2*NUM_MASSES_PER_BALL) {
			color = rgb_scale(0.5, -0.6 + 1.2/NUM_MASSES_PER_BALL * (i-NUM_MASSES_PER_BALL));
		}
		else {
			color = rgb_scale(1.0, -0.6 + 1.2/NUM_MASSES_PER_BALL * (i-2*NUM_MASSES_PER_BALL));
		}
		jbplot_trace_set_line_props(vel_traces[i], LINETYPE_SOLID, 2.0, &color);
		sprintf(trace_name, "mass_%d", i);
		jbplot_trace_set_name(vel_traces[i], trace_name);
		jbplot_add_trace((jbplot *)vel_plot, vel_traces[i]);
		jbplot_trace_set_line_props(pos_traces[i], LINETYPE_SOLID, 2.0, &color);
		jbplot_trace_set_name(pos_traces[i], trace_name);
		jbplot_add_trace((jbplot *)pos_plot, pos_traces[i]);
	}


	gtk_main ();

	return 0;
}
