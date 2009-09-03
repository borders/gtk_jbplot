/**
 * jbplot.c
 *
 * A GTK+ widget that implements a plot
 *
 * Authors:
 *   James Borders
 */

#include <gtk/gtk.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#include "jbplot.h"
#include "jbplot-marshallers.h"


#define JBPLOT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), JBPLOT_TYPE, jbplotPrivate))

G_DEFINE_TYPE (jbplot, jbplot, GTK_TYPE_DRAWING_AREA);

static gboolean jbplot_expose (GtkWidget *plot, GdkEventExpose *event);

/*
static gboolean egg_clock_face_button_press (GtkWidget *clock,
					     GdkEventButton *event);
static gboolean egg_clock_face_button_release (GtkWidget *clock,
					       GdkEventButton *event);
static gboolean egg_clock_face_motion_notify (GtkWidget *clock,
					      GdkEventMotion *event);
*/

static gboolean jbplot_update (gpointer data);


/* Here are the plot structs */
#define MAX_NUM_MAJOR_TICS    50
#define MAJOR_TIC_LABEL_SIZE  100
#define MAX_NUM_TRACES    10

typedef struct box_size_t {
  float width;
  float height;
} box_size_t;


typedef struct axis_t {
  int type; // 0=x, 1=y
  char do_show_axis_label;
  char *axis_label;
  char is_axis_label_owner;
  double axis_label_font_size;
  struct box_size_t axis_label_size;
  char do_show_tic_labels;
  char do_autoscale;
  char do_loose_fit;
  char do_show_major_gridlines;
  float major_gridline_width;
  char do_show_minor_gridlines;
  float major_tic_values[MAX_NUM_MAJOR_TICS];
  char *major_tic_labels[MAX_NUM_MAJOR_TICS];
  char is_tic_label_owner;
  float min_val;
  float max_val;
  char tic_label_format_string[100];
  double tic_label_font_size;
  int num_actual_major_tics;
  int num_request_major_tics;
  int num_minor_tics_per_major;
} axis_t;

typedef struct data_range {
  float min;
  float max;
} data_range;

typedef struct plot_area_t {
  char do_show_bounding_box;
  float bounding_box_width;
} plot_area_t;

typedef struct trace_t {
  float *x_data;
  float *y_data;
  int capacity;
  int length;
  int start_index;
  int end_index;
  int is_data_owner;
} trace_t;


typedef struct plot_t {
  struct plot_area_t plot_area;
  struct axis_t x_axis;
  struct axis_t y_axis;
  char do_show_plot_title;
  char *plot_title;
  char is_plot_title_owner;
  double plot_title_font_size;
  struct box_size_t plot_title_size;  
  
  struct trace_t *traces[MAX_NUM_TRACES];
  int num_traces;
} plot_t;


/* private (static) plotting utility functions */
static double get_text_height(cairo_t *cr, char *text, double font_size);
static double get_text_width(cairo_t *cr, char *text, double font_size);
static int set_major_tic_values(axis_t *a, double min, double max);
static int set_major_tic_labels(axis_t *a);
static double get_widest_label_width(axis_t *a, cairo_t *cr);
static void get_float_parts(double f, double *mantissa, int *exponent);
static double round_to_nearest(double num, double nearest);
static double round_up_to_nearest(double num, double nearest);
static double round_down_to_nearest(double num, double nearest);
static data_range get_y_range(trace_t **traces, int num_traces);
static data_range get_x_range(trace_t **traces, int num_traces);




typedef struct _jbplotPrivate jbplotPrivate;

struct _jbplotPrivate
{
	plot_t plot;
	gboolean dragging; /* true if the interface is being dragged */
};

enum
{
	TIME_CHANGED,
	LAST_SIGNAL
};

static guint jbplot_signals[LAST_SIGNAL] = { 0 };

static void jbplot_class_init (jbplotClass *class) {
	GObjectClass *obj_class;
	GtkWidgetClass *widget_class;

	obj_class = G_OBJECT_CLASS (class);
	widget_class = GTK_WIDGET_CLASS (class);

	/* GtkWidget signals */
	widget_class->expose_event = jbplot_expose;
/*
	widget_class->button_press_event = egg_clock_face_button_press;
	widget_class->button_release_event = egg_clock_face_button_release;
	widget_class->motion_notify_event = egg_clock_face_motion_notify;
*/

	/* jbplot signals */
/*
	egg_clock_face_signals[TIME_CHANGED] = g_signal_new (
			"time-changed",
			G_OBJECT_CLASS_TYPE (obj_class),
			G_SIGNAL_RUN_FIRST,
			G_STRUCT_OFFSET (EggClockFaceClass, time_changed),
			NULL, NULL,
			_clock_marshal_VOID__INT_INT,
			G_TYPE_NONE, 2,
			G_TYPE_INT,
			G_TYPE_INT);
*/

	g_type_class_add_private (obj_class, sizeof (jbplotPrivate));
}

static int init_axis(axis_t *axis) {
	int i;
  axis->do_show_axis_label = 1;
  axis->axis_label = "axis_label";
	axis->is_axis_label_owner = 0;
  axis->do_show_tic_labels = 1;
  axis->do_autoscale = 0;
  axis->do_loose_fit = 1;
  axis->do_show_major_gridlines = 1;
  axis->major_gridline_width = 1.0;
  axis->do_show_minor_gridlines = 0;
	for(i=0; i<MAX_NUM_MAJOR_TICS; i++) {
		axis->major_tic_labels[i] = malloc(MAJOR_TIC_LABEL_SIZE);
		if(axis->major_tic_labels[i] == NULL) {
			return -1;
		}
	}
  axis->is_tic_label_owner = 1;
  axis->min_val = 0.0;
  axis->max_val = 10.0;
  strcpy(axis->tic_label_format_string, "%g");
  axis->num_request_major_tics = 10;
  axis->num_minor_tics_per_major = 5;
  axis->tic_label_font_size = 10.;
  axis->axis_label_font_size = 10.;
	return 0;
}

static int init_plot_area(plot_area_t *area) {
  area->do_show_bounding_box = 1;
  area->bounding_box_width = 1.0;
	return 0;
}


static int init_plot(plot_t *plot) {

	if(	init_axis(&(plot->x_axis)) 
							||
			init_axis(&(plot->y_axis)) 
							||
			init_plot_area(&(plot->plot_area))
		) {
		return -1;
	}
  plot->do_show_plot_title = 0;
  plot->plot_title = "";
	plot->is_plot_title_owner = 0;
	plot->plot_title_font_size = 12.;
  
  plot->num_traces = 0;
	return 0;
}




static void jbplot_init (jbplot *plot) {
	gtk_widget_add_events (GTK_WIDGET (plot),
			GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
			GDK_POINTER_MOTION_MASK);

/*
	egg_clock_face_update (clock);

	// update the clock once a second 
	g_timeout_add (1000, egg_clock_face_update, clock);
*/

	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);

	// initialize the plot struct elements
	init_plot(&(priv->plot));

	//jbplot_set_plot_title((GtkWidget *)plot, "Test Title", 1);
	jbplot_set_x_axis_label(plot, "new x-axis label", 1);
	jbplot_set_y_axis_label(plot, "new y-axis label", 1);
	
}


typedef enum {
	ANCHOR_TOP_LEFT,
	ANCHOR_TOP_MIDDLE,
	ANCHOR_TOP_RIGHT,
	ANCHOR_MIDDLE_LEFT,
	ANCHOR_MIDDLE_MIDDLE,
	ANCHOR_MIDDLE_RIGHT,
	ANCHOR_BOTTOM_LEFT,
	ANCHOR_BOTTOM_MIDDLE,
	ANCHOR_BOTTOM_RIGHT
} anchor_t;


static int draw_vert_text_at_point(GtkWidget *plot, void *cr, char *text, double x, double y, anchor_t anchor) {
	double x_right, y_bottom;
	cairo_text_extents_t te;
	double w, h;

	cairo_save(cr);

	cairo_text_extents(cr, text, &te);
	w = te.width;
	h = te.height;
	switch(anchor) {
		case ANCHOR_TOP_LEFT:
			x_right = x + h;
			y_bottom = y + w;
			break;
		case ANCHOR_TOP_MIDDLE:
			x_right = x + h/2;
			y_bottom = y + w;
			break;
		case ANCHOR_TOP_RIGHT:
			x_right = x;
			y_bottom = y + w;
			break;
		case ANCHOR_MIDDLE_LEFT:
			x_right = x +  h;
			y_bottom = y + w/2;
			break;
		case ANCHOR_MIDDLE_MIDDLE:
			x_right = x + h/2;
			y_bottom = y + w/2;
			break;
		case ANCHOR_MIDDLE_RIGHT:
			x_right = x;
			y_bottom = y + w/2;
			break;
		case ANCHOR_BOTTOM_LEFT:
			x_right = x + h;
			y_bottom = y;
			break;
		case ANCHOR_BOTTOM_MIDDLE:
			x_right = x + h/2;
			y_bottom = y;
			break;
		case ANCHOR_BOTTOM_RIGHT:
			x_right = x;
			y_bottom = y;
			break;
		default:
			x_right = x;
			y_bottom = y;
	}
	cairo_translate(cr, x_right, y_bottom);
	cairo_rotate(cr, -M_PI/2);
	cairo_move_to(cr, 0.0, 0.0);
	cairo_show_text(cr, text);
	cairo_restore(cr);
	return 0;
}


static int draw_horiz_text_at_point(GtkWidget *plot, void *cr, char *text, double x, double y, anchor_t anchor) {
	double x_left, y_bottom;
	cairo_text_extents_t te;
	double w, h;

	cairo_save(cr);

	cairo_text_extents(cr, text, &te);
	w = te.width;
	h = te.height;
	switch(anchor) {
		case ANCHOR_TOP_LEFT:
			x_left = x;
			y_bottom = y + h;
			break;
		case ANCHOR_TOP_MIDDLE:
			x_left = x - w/2;
			y_bottom = y + h;
			break;
		case ANCHOR_TOP_RIGHT:
			x_left = x - w;
			y_bottom = y + h;
			break;
		case ANCHOR_MIDDLE_LEFT:
			x_left = x;
			y_bottom = y + h/2;
			break;
		case ANCHOR_MIDDLE_MIDDLE:
			x_left = x - w/2;
			y_bottom = y + h/2;
			break;
		case ANCHOR_MIDDLE_RIGHT:
			x_left = x - w;
			y_bottom = y + h/2;
			break;
		case ANCHOR_BOTTOM_LEFT:
			x_left = x;
			y_bottom = y;
			break;
		case ANCHOR_BOTTOM_MIDDLE:
			x_left = x - w/2;
			y_bottom = y;
			break;
		case ANCHOR_BOTTOM_RIGHT:
			x_left = x - w;
			y_bottom = y;
			break;
		default:
			x_left = x;
			y_bottom = y;
	}
	cairo_move_to(cr, x_left, y_bottom);
	cairo_show_text(cr, text);
	cairo_restore(cr);
	return 0;
}

static gboolean jbplot_expose (GtkWidget *plot, GdkEventExpose *event) {
	double width, height;
	int i, j;
	jbplotPrivate	*priv = JBPLOT_GET_PRIVATE(plot);
	plot_t *p = &(priv->plot);
	axis_t *x_axis = &(p->x_axis);
	axis_t *y_axis = &(p->y_axis);

	width = plot->allocation.width;
	height = plot->allocation.height;

	cairo_t *cr;
	cr = gdk_cairo_create(plot->window);

	// set some default values in cairo context
	cairo_set_line_width(cr, 1.0);

	//First fill the background
	cairo_rectangle(cr, 0., 0., width, height);
	cairo_set_source_rgb (cr, 0.9, 0.9, 0.9);
	cairo_fill(cr);

	// now do the layout calcs
  double title_top_edge = 0.01 * height;
  double title_bottom_edge = title_top_edge + get_text_height(cr, p->plot_title, p->plot_title_font_size);
  double y_label_left_edge = 0.01 * width;
  double y_label_right_edge = y_label_left_edge + 
                              get_text_height(cr, y_axis->axis_label, y_axis->axis_label_font_size);
  double x_label_bottom_edge = height - 0.01*height;
  double x_label_top_edge = x_label_bottom_edge - 
                             get_text_height(cr, x_axis->axis_label , x_axis->axis_label_font_size);

  // draw the plot title if desired	
	cairo_set_source_rgb (cr, 0., 0., 0.);
  if(p->do_show_plot_title) {
		cairo_save(cr);
		cairo_set_font_size(cr, p->plot_title_font_size);
		draw_horiz_text_at_point(plot, cr, p->plot_title, 0.5*width, title_top_edge, ANCHOR_TOP_MIDDLE);
		cairo_restore(cr);
  }

	// calculate data ranges and tic labels
  data_range x_range, y_range;
  x_range = get_x_range(p->traces, p->num_traces);
  y_range = get_y_range(p->traces, p->num_traces);
  set_major_tic_values(x_axis, x_range.min, x_range.max);
  set_major_tic_values(y_axis, y_range.min, y_range.max);
  set_major_tic_labels(x_axis);
  set_major_tic_labels(y_axis);
  double max_y_label_width = get_widest_label_width(y_axis, cr);
  double y_tic_labels_left_edge = y_label_right_edge + 0.01 * width;
  double y_tic_labels_right_edge = y_tic_labels_left_edge + max_y_label_width;
  double plot_area_left_edge = y_tic_labels_right_edge + 0.01 * width;
  double plot_area_right_edge = width - 0.06 * width;
  double plot_area_top_edge = title_bottom_edge + 0.02 * height;
  double x_tic_labels_bottom_edge = x_label_top_edge - 0.01 * height;
  double x_tic_labels_height = get_text_height(cr, x_axis->major_tic_labels[0], x_axis->tic_label_font_size);
  double x_tic_labels_top_edge = x_tic_labels_bottom_edge - x_tic_labels_height;
  double plot_area_bottom_edge = x_tic_labels_top_edge - 0.01 * height;
  double plot_area_height = plot_area_bottom_edge - plot_area_top_edge;
  double plot_area_width = plot_area_right_edge - plot_area_left_edge;
  double y_label_middle_y = plot_area_top_edge + plot_area_height/2;
  double y_label_bottom_edge = y_label_middle_y + 0.5*get_text_width(cr, y_axis->axis_label, y_axis->axis_label_font_size);
  double x_label_middle_x = plot_area_left_edge + plot_area_width/2;

	// these params can be used to transform from data coordinates to pixel coords
	// for x-direction, use: x_pixel = x_m * x_data + x_b
	// for y-direction, use: y_pixel = y_m * y_data + y_b	
	double x_m = (plot_area_right_edge - plot_area_left_edge) / (x_axis->max_val - x_axis->min_val);	
	double x_b = plot_area_left_edge - x_m * x_axis->min_val;	
	double y_m = (plot_area_top_edge - plot_area_bottom_edge) / (y_axis->max_val - y_axis->min_val);	
	double y_b = plot_area_bottom_edge - y_m * y_axis->min_val;		
	
	// fill the plot area (we'll stroke the border later)
	cairo_set_source_rgb (cr, 1, 1, 1);
	cairo_rectangle(	cr, 
										plot_area_left_edge,
										plot_area_top_edge,
										(plot_area_right_edge-plot_area_left_edge),
										(plot_area_bottom_edge-plot_area_top_edge)
									);
	cairo_fill(cr);

	// draw the y tic labels
	cairo_set_source_rgb (cr, 0., 0., 0.);
	cairo_save(cr);
	cairo_set_font_size(cr, y_axis->tic_label_font_size);
	for(i=0; i<y_axis->num_actual_major_tics; i++) {
		draw_horiz_text_at_point(	plot,
															cr, 
															y_axis->major_tic_labels[i], 
															y_tic_labels_right_edge, 
															y_m * y_axis->major_tic_values[i] + y_b, 
															ANCHOR_MIDDLE_RIGHT
														);
	}
	cairo_restore(cr);

	// draw the y major gridlines
	cairo_set_source_rgb (cr, 0.6, 0.6, 0.6);
	for(i=0; i<y_axis->num_actual_major_tics; i++) {
		double y = y_m * y_axis->major_tic_values[i] + y_b;
		cairo_move_to(cr, plot_area_left_edge, y);
		cairo_line_to(cr, plot_area_right_edge, y);
		cairo_stroke(cr);
	}	
	
	// draw the x tic labels
	cairo_set_source_rgb (cr, 0., 0., 0.);
	cairo_save(cr);
	cairo_set_font_size(cr, x_axis->tic_label_font_size);
	for(i=0; i<x_axis->num_actual_major_tics; i++) {
		draw_horiz_text_at_point(	plot,
															cr, 
															x_axis->major_tic_labels[i], 
															x_m * x_axis->major_tic_values[i] + x_b, 
															x_tic_labels_top_edge, 
															ANCHOR_TOP_MIDDLE
														);
	}
	cairo_restore(cr);

	// draw the x major gridlines
	cairo_set_source_rgb (cr, 0.6, 0.6, 0.6);
	for(i=0; i<x_axis->num_actual_major_tics; i++) {
		double x = x_m * x_axis->major_tic_values[i] + x_b;		
		cairo_move_to(cr, x, plot_area_bottom_edge);
		cairo_line_to(cr, x, plot_area_top_edge);
		cairo_stroke(cr);
	}	
			
	// draw the y-axis label if desired
	cairo_set_source_rgb (cr, 0, 0, 0);
	if(y_axis->do_show_axis_label) {
		draw_vert_text_at_point(	plot,
															cr, 
															y_axis->axis_label, 
															y_label_left_edge, 
															y_label_bottom_edge, 
															ANCHOR_BOTTOM_LEFT
														);
	}
	
	// draw the x-axis label if desired
	cairo_set_source_rgb (cr, 0, 0, 0);
	if(x_axis->do_show_axis_label) {
		draw_horiz_text_at_point(	plot,
															cr, 
															x_axis->axis_label, 
															x_label_middle_x, 
															x_label_top_edge, 
															ANCHOR_TOP_MIDDLE
														);
	}
	
	// draw the data!!!
	cairo_set_source_rgb (cr, 1.0, 0, 0);
	for(i = 0; i < p->num_traces; i++) {
		trace_t *t = p->traces[i];
		if(t->length <= 0) continue;
		cairo_move_to(
			cr, 
			x_m * t->x_data[t->start_index] + x_b,
			y_m * t->y_data[t->start_index] + y_b
		);
		for(j = 1; j < t->length; j++) {
			int n;
			n = t->start_index + j;
			if(n >= t->capacity) {
				n -= t->capacity;
			}
			cairo_line_to(
				cr,
				x_m * t->x_data[n] + x_b, 
				y_m * t->y_data[n] + y_b
			);
		}
		cairo_stroke(cr);
	}

	// draw the plot area border
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_rectangle(
		cr, 
		plot_area_left_edge, 
		plot_area_top_edge,
		plot_area_right_edge - plot_area_left_edge,
		plot_area_bottom_edge - plot_area_top_edge
	);
	cairo_stroke(cr);	


	cairo_destroy(cr);

	return FALSE;
}

static void jbplot_redraw_canvas (jbplot *plot) {
	GtkWidget *widget;
	GdkRegion *region;
	
	widget = GTK_WIDGET (plot);

	if (!widget->window) return;

	region = gdk_drawable_get_clip_region (widget->window);
	/* redraw the cairo canvas completely by exposing it */
	gdk_window_invalidate_region (widget->window, region, TRUE);
	gdk_window_process_updates (widget->window, TRUE);

	gdk_region_destroy (region);
}



static gboolean jbplot_update (gpointer data) {
	jbplot *plot;
	jbplotPrivate *priv;

	plot = JBPLOT(data);
	priv = JBPLOT_GET_PRIVATE(plot);
	
	jbplot_redraw_canvas (plot);

	return TRUE; /* keep running this event */
}



/******************* Plot Drawing Functions **************************/
static int set_major_tic_values(axis_t *a, double min, double max) {
	double raw_range = max - min;
	double raw_tic_delta = raw_range / (a->num_request_major_tics - 1);
	double mantissa;
	int exponent;
	get_float_parts(raw_tic_delta, &mantissa, &exponent);
	double actual_tic_delta;

	if(mantissa <= 1.0) {
		actual_tic_delta = 1.0 * pow(10., exponent);
	}
	else if(mantissa <= 2.0) {
		actual_tic_delta = 2.0 * pow(10., exponent);
	}
	else if(mantissa <= 2.5) {
		actual_tic_delta = 2.5 * pow(10., exponent);
	}
	else if(mantissa <= 5.0) {
		actual_tic_delta = 5.0 * pow(10., exponent);
	}
	else {
		actual_tic_delta = 1.0 * pow(10., exponent+1);
	}

	double min_tic_val;
	if(a->do_loose_fit) {
		min_tic_val = round_down_to_nearest(min, actual_tic_delta);
		a->min_val = min_tic_val;
	}
	else {
		min_tic_val = round_up_to_nearest(min, actual_tic_delta);
		a->min_val = min;
		a->max_val = max;
	}
		
	double tic_val;
	int i = 0;
	for(tic_val = min_tic_val; tic_val < max && i < MAX_NUM_MAJOR_TICS; tic_val += actual_tic_delta) {
		a->major_tic_values[i] = tic_val;
		i++;
	}
	if(i >= MAX_NUM_MAJOR_TICS) {
		printf("Too many major tics!!!\n");
		return -1;
	}
	
	if(a->do_loose_fit) {
		a->major_tic_values[i] = tic_val;
		a->max_val = tic_val;	
		a->num_actual_major_tics = i+1;
	}
	else {
		a->max_val = max;		
		a->num_actual_major_tics = i;		
	}
	
	return 0;
}


static int set_major_tic_labels(axis_t *a) {
	int i, ret;
	int err = 0;
	for(i=0; i<a->num_actual_major_tics; i++) {
		ret = snprintf(a->major_tic_labels[i], MAJOR_TIC_LABEL_SIZE, a->tic_label_format_string, a->major_tic_values[i]);
		if(ret > MAJOR_TIC_LABEL_SIZE) {
			err = -1;
		}
	}
	return err;
}





static double get_text_height(cairo_t *cr, char *text, double font_size) {
	cairo_text_extents_t te;
	cairo_save(cr);
	cairo_set_font_size(cr, font_size);
	cairo_text_extents(cr, text, &te);
	cairo_restore(cr);
	return te.height;
}
	
static double get_text_width(cairo_t *cr, char *text, double font_size) {
	cairo_text_extents_t te;
	cairo_save(cr);
	cairo_set_font_size(cr, font_size);
	cairo_text_extents(cr, text, &te);
	cairo_restore(cr);
	return te.width;
}
	


static double get_widest_label_width(axis_t *a, cairo_t *cr) {
  double max = 0.0;
  double w;
	cairo_text_extents_t te;
  int i;
  for(i=0; i<a->num_actual_major_tics; i++) {
		cairo_text_extents(cr, a->major_tic_labels[i], &te);
    w = te.width;
    if(w > max) {
      max = w;
    }
  }
  return max;
}
      

static void get_float_parts(double f, double *mantissa, int *exponent) {
  int neg = 0;
  if(f == 0.0) {  
    *mantissa = 0.0;
    *exponent = 0;
    return;
  }
  if(f < 0) neg = 1;
  *exponent = floor(log10(fabs(f)));
  *mantissa = f / pow(10, *exponent);
  return;
}


static double round_to_nearest(double num, double nearest) {
  double a = num / nearest;
  return (floor(a + 0.5) * nearest);
}

static double round_up_to_nearest(double num, double nearest) {
  double a = num / nearest;
  return (ceil(a) * nearest);
}

static double round_down_to_nearest(double num, double nearest) {
  double a = num / nearest;
  return (floor(a) * nearest);
}


static data_range get_y_range(trace_t **traces, int num_traces) {
  data_range r;
  int i, j;
  float min = FLT_MAX, max = FLT_MIN;
  for(i = 0; i < num_traces; i++) {
    trace_t *t = traces[i];
    for(j = 0; j< t->length; j++) {
      if(t->y_data[j] > max) {
        max = t->y_data[j];
      }
      if(t->y_data[j] < min) {
        min = t->y_data[j];
      }
    }
  }
  r.min = min;
  r.max = max;
  return r;
}

static data_range get_x_range(trace_t **traces, int num_traces) {
  data_range r;
  int i, j;
  float min = FLT_MAX, max = FLT_MIN;
  for(i = 0; i < num_traces; i++) {
    trace_t *t = traces[i];
    for(j = 0; j< t->length; j++) {
      if(t->x_data[j] > max) {
        max = t->x_data[j];
      }
      if(t->x_data[j] < min) {
        min = t->x_data[j];
      }
    }
  }
  r.min = min;
  r.max = max;
  return r;
}

trace_t *trace_create_with_external_data(float *x, float *y, int length, int capacity) {
	trace_t *t;
	t = malloc(sizeof(trace_t));
	if(t==NULL) {
		printf("Error allocating trace_t structure\n");
		return NULL;
	}
	t->x_data = x;
	t->y_data = y;
	t->capacity = capacity;
	t->length = length;
	t->start_index = 0;
	t->end_index = length - 1;
	t->is_data_owner = 0;
	return t;
}

/******************** Public Functions *******************************/
int jbplot_add_trace(jbplot *plot, float *x, float *y, int length, int capacity) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	plot_t *p = &(priv->plot);

	if(p->num_traces + 1 >= MAX_NUM_TRACES) {
		return -1;
	}
	trace_t *t = trace_create_with_external_data(x, y, length, capacity);
	if(t == NULL) {
		printf("Error creating trace...\n");
		return -1;
	}
	p->traces[p->num_traces] = t;
	(p->num_traces)++;
	return (p->num_traces)-1;
}


GtkWidget *jbplot_new (void) {
	return g_object_new (JBPLOT_TYPE, NULL);
}


int jbplot_set_plot_title(jbplot *plot, char *title, int copy) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);

	if(copy) {
		if(priv->plot.is_plot_title_owner) {
			free(priv->plot.plot_title);
		}
		priv->plot.plot_title = malloc(strlen(title)+1);
		if(priv->plot.plot_title == NULL) {
			return -1;
		}
		strcpy(priv->plot.plot_title, title);
		priv->plot.is_plot_title_owner = 1;
	}
	else {
		priv->plot.plot_title = title;
		priv->plot.is_plot_title_owner = 0;
	}
	priv->plot.do_show_plot_title = 1;

	return 0;
}

int jbplot_set_x_axis_label(jbplot *plot, char *title, int copy) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);

	if(copy) {
		if(priv->plot.x_axis.is_axis_label_owner) {
			free(priv->plot.x_axis.axis_label);
		}
		priv->plot.x_axis.axis_label = malloc(strlen(title)+1);
		if(priv->plot.x_axis.axis_label == NULL) {
			return -1;
		}
		strcpy(priv->plot.x_axis.axis_label, title);
		priv->plot.x_axis.is_axis_label_owner = 1;
	}
	else {
		priv->plot.x_axis.axis_label = title;
		priv->plot.x_axis.is_axis_label_owner = 0;
	}
	return 0;
}
	
int jbplot_set_y_axis_label(jbplot *plot, char *title, int copy) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);

	if(copy) {
		if(priv->plot.y_axis.is_axis_label_owner) {
			free(priv->plot.y_axis.axis_label);
		}
		priv->plot.y_axis.axis_label = malloc(strlen(title)+1);
		if(priv->plot.y_axis.axis_label == NULL) {
			return -1;
		}
		strcpy(priv->plot.y_axis.axis_label, title);
		priv->plot.y_axis.is_axis_label_owner = 1;
	}
	else {
		priv->plot.y_axis.axis_label = title;
		priv->plot.y_axis.is_axis_label_owner = 0;
	}
	return 0;
}
	
