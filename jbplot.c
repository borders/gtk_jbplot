/*
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
static gboolean jbplot_update (gpointer data);


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
	rgb_color_t major_gridline_color;
	int major_gridline_type;
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
	float left_edge;
	float right_edge;
	float top_edge;
	float bottom_edge;
	rgb_color_t bg_color;
	rgb_color_t border_color;
} plot_area_t;

typedef struct trace_t {
  float *x_data;
  float *y_data;
  int capacity;
  int length;
  int start_index;
  int end_index;
  int is_data_owner;
	float line_width;
	int line_type;
	rgb_color_t line_color;
	rgb_color_t marker_color;
	int marker_type;
	float marker_size;
} trace_t;


typedef struct plot_t {
	rgb_color_t bg_color;
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
	gboolean zooming; 
	gboolean panning; 
	gboolean do_show_coords;
	gboolean do_show_cross_hair;
	gdouble drag_start_x;
	gdouble drag_start_y;
	gdouble drag_end_x;
	gdouble drag_end_y;
	gdouble pan_start_x;
	gdouble pan_start_y;
	data_range pan_start_x_range;
	data_range pan_start_y_range;

	/* these are used to convert from device coords (pixels) to data coords */
	double x_m;
	double x_b;
	double y_m;
	double y_b;
};

enum
{
	TIME_CHANGED,
	LAST_SIGNAL
};

static guint jbplot_signals[LAST_SIGNAL] = { 0 };


static gboolean popup_responder(GtkWidget *w, GdkEvent *e, gpointer data) {
	printf("Action not yet implemented (%s)\n", (char *)data);
	return FALSE;
}

static gboolean popup_callback_show_cross_hair(GtkWidget *w, GdkEvent *e, gpointer data) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE((jbplot *) data);
	printf("Toggling show_cross_hair state\n");
	priv->do_show_cross_hair = !(priv->do_show_cross_hair);
	gtk_widget_queue_draw((GtkWidget *)data);
	return FALSE;
}

static gboolean popup_callback_show_coords(GtkWidget *w, GdkEvent *e, gpointer data) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE((jbplot *) data);
	printf("Toggling show_coords state\n");
	priv->do_show_coords = !(priv->do_show_coords);
	gtk_widget_queue_draw((GtkWidget *)data);
	return FALSE;
}


static gboolean popup_callback_clear(GtkWidget *w, GdkEvent *e, gpointer data) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE((jbplot *) data);
	printf("Clearing plot (FIXME!!!!)\n");
	if(priv->plot.num_traces > 0) {
		trace_t *t = priv->plot.traces[0];
		t->length = 0;
		t->start_index = 0;
		t->end_index = 0;
		gtk_widget_queue_draw((GtkWidget *)data);
	}
	return FALSE;
}

static gboolean popup_callback_x_autoscale(GtkWidget *w, GdkEvent *e, gpointer data) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE((jbplot *) data);
	printf("Toggling x-axis autoscale state\n");
	priv->plot.x_axis.do_autoscale = !(priv->plot.x_axis.do_autoscale);
	gtk_widget_queue_draw((GtkWidget *)data);
	return FALSE;
}

static gboolean popup_callback_x_loose_fit(GtkWidget *w, GdkEvent *e, gpointer data) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE((jbplot *) data);
	printf("Toggling x-axis loose fit state\n");
	priv->plot.x_axis.do_loose_fit = !(priv->plot.x_axis.do_loose_fit);
	gtk_widget_queue_draw((GtkWidget *)data);
	return FALSE;
}

static gboolean popup_callback_y_autoscale(GtkWidget *w, GdkEvent *e, gpointer data) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE((jbplot *) data);
	printf("Toggling y-axis autoscale state\n");
	priv->plot.y_axis.do_autoscale = !(priv->plot.y_axis.do_autoscale);
	gtk_widget_queue_draw((GtkWidget *)data);
	return FALSE;
}

static gboolean popup_callback_y_loose_fit(GtkWidget *w, GdkEvent *e, gpointer data) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE((jbplot *) data);
	printf("Toggling y-axis loose fit state\n");
	priv->plot.y_axis.do_loose_fit = !(priv->plot.y_axis.do_loose_fit);
	gtk_widget_queue_draw((GtkWidget *)data);
	return FALSE;
}
static void do_popup_menu (GtkWidget *my_widget, GdkEventButton *event) {
  GtkWidget *menu;
  GtkWidget *x_axis_submenu;
  GtkWidget *y_axis_submenu;
  int button, event_time;

	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(my_widget);

  menu = gtk_menu_new ();
  g_signal_connect (menu, "deactivate", 
                    G_CALLBACK (gtk_widget_destroy), NULL);


	GtkWidget *clear_plot = gtk_menu_item_new_with_label("Clear Plot");
	GtkWidget *x = gtk_menu_item_new_with_label("x-axis");
	GtkWidget *y = gtk_menu_item_new_with_label("y-axis");
	GtkWidget *show_coords = gtk_check_menu_item_new_with_label("Show Coords");
	GtkWidget *show_cross_hair = gtk_check_menu_item_new_with_label("Show Crosshair");

	if(priv->do_show_coords) {
		gtk_check_menu_item_set_active((GtkCheckMenuItem *)show_coords, TRUE);
	}

	if(priv->do_show_cross_hair) {
		gtk_check_menu_item_set_active((GtkCheckMenuItem *)show_cross_hair, TRUE);
	}

	g_signal_connect(G_OBJECT(clear_plot), "button-press-event", G_CALLBACK(popup_callback_clear), (gpointer) my_widget);
	g_signal_connect(G_OBJECT(x), "button-press-event", G_CALLBACK(popup_responder), (gpointer) "x-axis");
	g_signal_connect(G_OBJECT(y), "button-press-event", G_CALLBACK(popup_responder), (gpointer) "y-axis");
	g_signal_connect(G_OBJECT(show_coords), "button-press-event", G_CALLBACK(popup_callback_show_coords), (gpointer) my_widget);
	g_signal_connect(G_OBJECT(show_cross_hair), "button-press-event", G_CALLBACK(popup_callback_show_cross_hair), (gpointer) my_widget);

	gtk_menu_shell_append((GtkMenuShell *)menu, clear_plot);
	gtk_menu_shell_append((GtkMenuShell *)menu, x);
	gtk_menu_shell_append((GtkMenuShell *)menu, y);
	gtk_menu_shell_append((GtkMenuShell *)menu, show_coords);
	gtk_menu_shell_append((GtkMenuShell *)menu, show_cross_hair);

	/* x-axis submenu */
  x_axis_submenu = gtk_menu_new();
	GtkWidget *x_format = gtk_menu_item_new_with_label("Format");
	GtkWidget *x_autoscale = gtk_check_menu_item_new_with_label("Autoscale");
	GtkWidget *x_loose_fit = gtk_check_menu_item_new_with_label("Loose Fit");

	g_signal_connect(G_OBJECT(x_format), "button-press-event", G_CALLBACK(popup_responder), (gpointer) "x:format");
	g_signal_connect(G_OBJECT(x_autoscale), "button-press-event", G_CALLBACK(popup_callback_x_autoscale), (gpointer) my_widget);
	g_signal_connect(G_OBJECT(x_loose_fit), "button-press-event", G_CALLBACK(popup_callback_x_loose_fit), (gpointer) my_widget);

	if(priv->plot.x_axis.do_autoscale) {
		gtk_check_menu_item_set_active((GtkCheckMenuItem *)x_autoscale, TRUE);
		if(priv->plot.x_axis.do_loose_fit) {
			gtk_check_menu_item_set_active((GtkCheckMenuItem *)x_loose_fit, TRUE);
		}
	}
	else {
		gtk_widget_set_sensitive(x_loose_fit, FALSE);
	}

	gtk_menu_shell_append((GtkMenuShell *)x_axis_submenu, x_format);
	gtk_menu_shell_append((GtkMenuShell *)x_axis_submenu, x_autoscale);
	gtk_menu_shell_append((GtkMenuShell *)x_axis_submenu, x_loose_fit);
	gtk_widget_show(x_format);
	gtk_widget_show(x_autoscale);
	gtk_widget_show(x_loose_fit);

	/* y-axis submenu */
  y_axis_submenu = gtk_menu_new();
	GtkWidget *y_format = gtk_menu_item_new_with_label("Format");
	GtkWidget *y_autoscale = gtk_check_menu_item_new_with_label("Autoscale");
	GtkWidget *y_loose_fit = gtk_check_menu_item_new_with_label("Loose Fit");

	g_signal_connect(G_OBJECT(y_format), "button-press-event", G_CALLBACK(popup_responder), (gpointer) "y:format");
	g_signal_connect(G_OBJECT(y_autoscale), "button-press-event", G_CALLBACK(popup_callback_y_autoscale), (gpointer) my_widget);
	g_signal_connect(G_OBJECT(y_loose_fit), "button-press-event", G_CALLBACK(popup_callback_y_loose_fit), (gpointer) my_widget);

	if(priv->plot.y_axis.do_autoscale) {
		gtk_check_menu_item_set_active((GtkCheckMenuItem *)y_autoscale, TRUE);
		if(priv->plot.y_axis.do_loose_fit) {
			gtk_check_menu_item_set_active((GtkCheckMenuItem *)y_loose_fit, TRUE);
		}
	}
	else {
		gtk_widget_set_sensitive(y_loose_fit, FALSE);
	}

	gtk_menu_shell_append((GtkMenuShell *)y_axis_submenu, y_format);
	gtk_menu_shell_append((GtkMenuShell *)y_axis_submenu, y_autoscale);
	gtk_menu_shell_append((GtkMenuShell *)y_axis_submenu, y_loose_fit);
	gtk_widget_show(y_format);
	gtk_widget_show(y_autoscale);
	gtk_widget_show(y_loose_fit);

	gtk_menu_item_set_submenu((GtkMenuItem *)x, x_axis_submenu);
	gtk_menu_item_set_submenu((GtkMenuItem *)y, y_axis_submenu);

	gtk_widget_show(clear_plot);
	gtk_widget_show(x);
	gtk_widget_show(y);
	gtk_widget_show(show_coords);
	gtk_widget_show(show_cross_hair);

  if (event)
    {
      button = event->button;
      event_time = event->time;
    }
  else
    {
      button = 0;
      event_time = gtk_get_current_event_time ();
    }

  gtk_menu_attach_to_widget (GTK_MENU (menu), my_widget, NULL);
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, button, event_time);
}



static gboolean jbplot_button_press(GtkWidget *w, GdkEventButton *event) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE((jbplot*)w);
	if(event->button == 3) {
		do_popup_menu(w, event);
	}
	else if(event->button == 1) {
		if(event->x >= priv->plot.plot_area.left_edge &&
		   event->x <= priv->plot.plot_area.right_edge &&
		   event->y >= priv->plot.plot_area.top_edge &&
		   event->y <= priv->plot.plot_area.bottom_edge
		) {
			priv->zooming = TRUE;
			priv->drag_start_x = event->x;
			priv->drag_start_y = event->y;
			priv->drag_end_x = event->x;
			priv->drag_end_y = event->y;
		}
	}
	else if(event->button == 2) {
		if(event->x >= priv->plot.plot_area.left_edge &&
		   event->x <= priv->plot.plot_area.right_edge &&
		   event->y >= priv->plot.plot_area.top_edge &&
		   event->y <= priv->plot.plot_area.bottom_edge
		) {
			priv->panning = TRUE;
			priv->pan_start_x = event->x;
			priv->pan_start_y = event->y;
			priv->pan_start_x_range.min = priv->plot.x_axis.min_val;
			priv->pan_start_x_range.max = priv->plot.x_axis.max_val;
			priv->pan_start_y_range.min = priv->plot.y_axis.min_val;
			priv->pan_start_y_range.max = priv->plot.y_axis.max_val;
			jbplot_set_x_axis_range((jbplot *)w, priv->plot.x_axis.min_val, priv->plot.x_axis.max_val); 
			jbplot_set_y_axis_range((jbplot *)w, priv->plot.y_axis.min_val, priv->plot.y_axis.max_val); 
		}
	}
	return FALSE;
}

static gboolean jbplot_button_release(GtkWidget *w, GdkEventButton *event) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE((jbplot*)w);
	if(event->button == 1) {
		if(priv->zooming) {
			double x_now, y_now, x_min, x_max, y_min, y_max;
			priv->zooming = FALSE;
			x_now = event->x;
			y_now = event->y;
			if(x_now != priv->drag_start_x && y_now != priv->drag_start_y) {
				x_min = (x_now < priv->drag_start_x) ? x_now : priv->drag_start_x;
				x_max = (x_now > priv->drag_start_x) ? x_now : priv->drag_start_x;
				y_min = (y_now < priv->drag_start_y) ? y_now : priv->drag_start_y;
				y_max = (y_now > priv->drag_start_y) ? y_now : priv->drag_start_y;
				jbplot_set_x_axis_range((jbplot *)w, (x_min - priv->x_b)/priv->x_m, (x_max - priv->x_b)/priv->x_m);
				jbplot_set_y_axis_range((jbplot *)w, (y_max - priv->y_b)/priv->y_m, (y_min - priv->y_b)/priv->y_m);
			}
			gtk_widget_queue_draw(w);
		}
	}
	else if(event->button == 2) {
		if(priv->panning) {
			priv->panning = FALSE;
			gtk_widget_queue_draw(w);
		}
	}
	return FALSE;
}


static gboolean jbplot_motion_notify(GtkWidget *w, GdkEventMotion *event) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE((jbplot*)w);
	if(priv->zooming) {
		priv->drag_end_x = event->x;
		priv->drag_end_y = event->y;
		gtk_widget_queue_draw(w);
	}
	if(priv->panning) {
		jbplot_set_x_axis_range((jbplot *)w, priv->pan_start_x_range.min - (event->x - priv->pan_start_x)/priv->x_m , priv->pan_start_x_range.max - (event->x - priv->pan_start_x)/priv->x_m);
		jbplot_set_y_axis_range((jbplot *)w, priv->pan_start_y_range.min - (event->y - priv->pan_start_y)/priv->y_m, priv->pan_start_y_range.max - (event->y - priv->pan_start_y)/priv->y_m);
		gtk_widget_queue_draw(w);
	}
	if(priv->do_show_coords) {
		gtk_widget_queue_draw(w);
	}
	if(priv->do_show_cross_hair) {
		gtk_widget_queue_draw(w);
	}
	return FALSE;
}


static void jbplot_class_init (jbplotClass *class) {
	GObjectClass *obj_class;
	GtkWidgetClass *widget_class;

	obj_class = G_OBJECT_CLASS (class);
	widget_class = GTK_WIDGET_CLASS (class);

	/* GtkWidget signals */
	widget_class->expose_event = jbplot_expose;
	widget_class->button_press_event = jbplot_button_press;
	widget_class->button_release_event = jbplot_button_release;
	widget_class->motion_notify_event = jbplot_motion_notify;

	g_type_class_add_private (obj_class, sizeof (jbplotPrivate));
}

static int init_axis(axis_t *axis) {
	int i;
	rgb_color_t color = {0.8, 0.8, 0.8};
  axis->do_show_axis_label = 1;
  axis->axis_label = "axis_label";
	axis->is_axis_label_owner = 0;
  axis->do_show_tic_labels = 1;
  axis->do_autoscale = 1;
  axis->do_loose_fit = 0;
  axis->do_show_major_gridlines = 1;
  axis->major_gridline_width = 1.0;
	axis->major_gridline_color = color;
	axis->major_gridline_type = LINETYPE_SOLID;
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
	rgb_color_t color = {0.0, 0.0, 0.0};
  area->do_show_bounding_box = 1;
  area->bounding_box_width = 1.0;
	area->border_color = color;
	color.red = color.green = color.blue = 1.0;
	area->bg_color = color;
	return 0;
}


static int init_plot(plot_t *plot) {
	rgb_color_t color = {1.0, 1.0, 1.0};

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
	plot->bg_color = color;
  
  plot->num_traces = 0;
	return 0;
}


static void jbplot_init (jbplot *plot) {
	gtk_widget_add_events (GTK_WIDGET (plot),
			GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
			GDK_POINTER_MOTION_MASK);

	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);

	priv->zooming = FALSE;
	priv->panning = FALSE;
	priv->do_show_coords = FALSE;
	priv->do_show_cross_hair = FALSE;

	// initialize the plot struct elements
	init_plot(&(priv->plot));

	jbplot_set_plot_title(plot, " ", 1);
	jbplot_set_x_axis_label(plot, " ", 1);
	jbplot_set_y_axis_label(plot, " ", 1);
	
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


void draw_marker(cairo_t *cr, int type, float size) {
	double x, y;
	cairo_get_current_point(cr, &x, &y);
	if(type == MARKER_CIRCLE) {
		cairo_arc(cr, x, y, size/2.0, 0, 2*M_PI);
		cairo_fill(cr);
	}
	else if(type == MARKER_SQUARE) {
		cairo_rectangle(cr, x-size/2.0, y-size/2.0, size, size);
		cairo_fill(cr);
	}
	return;
}


static gboolean jbplot_expose (GtkWidget *plot, GdkEventExpose *event) {
	double width, height;
	int i, j;
	jbplotPrivate	*priv = JBPLOT_GET_PRIVATE(plot);
	plot_t *p = &(priv->plot);
	axis_t *x_axis = &(p->x_axis);
	axis_t *y_axis = &(p->y_axis);
	plot_area_t *pa = &(p->plot_area);

	width = plot->allocation.width;
	height = plot->allocation.height;
	
	cairo_t *cr;
	cr = gdk_cairo_create(plot->window);

	// set some default values in cairo context
	cairo_set_line_width(cr, 1.0);

	//First fill the background
	cairo_rectangle(cr, 0., 0., width, height);
	cairo_set_source_rgb (cr, p->bg_color.red, p->bg_color.green, p->bg_color.blue);
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
	if(x_axis->do_autoscale) {
	  x_range = get_x_range(p->traces, p->num_traces);
	}
	else {
		x_range.min = x_axis->min_val;
		x_range.max = x_axis->max_val;
	}
	if(y_axis->do_autoscale) {
		y_range = get_y_range(p->traces, p->num_traces);
	}
	else {
		y_range.min = y_axis->min_val;
		y_range.max = y_axis->max_val;
	}
  set_major_tic_values(x_axis, x_range.min, x_range.max);
  set_major_tic_values(y_axis, y_range.min, y_range.max);
  set_major_tic_labels(x_axis);
  set_major_tic_labels(y_axis);
  double max_y_label_width = get_widest_label_width(y_axis, cr);
  double y_tic_labels_left_edge;
	if(y_axis->do_show_axis_label) {
  	y_tic_labels_left_edge = y_label_right_edge + 0.01 * width;
	}
	else {
  	y_tic_labels_left_edge = 0.01 * width;
	}

  double y_tic_labels_right_edge = y_tic_labels_left_edge + max_y_label_width;
  double plot_area_left_edge = y_tic_labels_right_edge + 0.01 * width;
	priv->plot.plot_area.left_edge = plot_area_left_edge;
  double plot_area_right_edge = width - 0.06 * width;
	priv->plot.plot_area.right_edge = plot_area_right_edge;
	double plot_area_top_edge;
	if(p->do_show_plot_title) {
	  plot_area_top_edge = title_bottom_edge + 0.02 * height;
	}
	else {
	  plot_area_top_edge = 0.02 * height;
	}
	priv->plot.plot_area.top_edge = plot_area_top_edge;
  double x_tic_labels_bottom_edge;
	if(x_axis->do_show_axis_label) {
		x_tic_labels_bottom_edge = x_label_top_edge - 0.01 * height;
	}
	else {
		x_tic_labels_bottom_edge = height - 0.01 * height;
	}

  double x_tic_labels_height = get_text_height(cr, x_axis->major_tic_labels[0], x_axis->tic_label_font_size);
  double x_tic_labels_top_edge = x_tic_labels_bottom_edge - x_tic_labels_height;
  double plot_area_bottom_edge = x_tic_labels_top_edge - 0.01 * height;
	priv->plot.plot_area.bottom_edge = plot_area_bottom_edge;
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
	priv->x_m = x_m;	
	priv->y_m = y_m;	
	priv->x_b = x_b;	
	priv->y_b = y_b;	
	
	// fill the plot area (we'll stroke the border later)
	cairo_set_source_rgb (cr, pa->bg_color.red, pa->bg_color.green, pa->bg_color.blue);
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
	if(y_axis->do_show_major_gridlines && y_axis->major_gridline_type != LINETYPE_NONE) {
		cairo_set_source_rgb(cr, 
		                     y_axis->major_gridline_color.red, 
		                     y_axis->major_gridline_color.green, 
		                     y_axis->major_gridline_color.blue
		);
		cairo_set_line_width(cr, y_axis->major_gridline_width);
		for(i=0; i<y_axis->num_actual_major_tics; i++) {
			double y = y_m * y_axis->major_tic_values[i] + y_b;
			cairo_move_to(cr, plot_area_left_edge, y);
			cairo_line_to(cr, plot_area_right_edge, y);
			cairo_stroke(cr);
		}	
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
	if(x_axis->do_show_major_gridlines && x_axis->major_gridline_type != LINETYPE_NONE) {
		cairo_set_source_rgb(cr, 
		                     x_axis->major_gridline_color.red, 
		                     x_axis->major_gridline_color.green, 
		                     x_axis->major_gridline_color.blue
		);
		cairo_set_line_width(cr, x_axis->major_gridline_width);
		for(i=0; i<x_axis->num_actual_major_tics; i++) {
			double x = x_m * x_axis->major_tic_values[i] + x_b;		
			cairo_move_to(cr, x, plot_area_bottom_edge);
			cairo_line_to(cr, x, plot_area_top_edge);
			cairo_stroke(cr);
		}	
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
	
	/*************** Draw the data ******************/

	// first set the clipping region
	cairo_save(cr);
	cairo_rectangle(	cr, 
										plot_area_left_edge,
										plot_area_top_edge,
										(plot_area_right_edge-plot_area_left_edge),
										(plot_area_bottom_edge-plot_area_top_edge)
									);
	cairo_clip(cr);

	// now draw the trace lines (if requested)
	for(i = 0; i < p->num_traces; i++) {
		char first_pt = 1;
		trace_t *t = p->traces[i];
		if(t->line_type == LINETYPE_NONE) {
			continue;
		}
		cairo_set_source_rgb (cr, t->line_color.red, t->line_color.green, t->line_color.blue);
		if(t->length <= 0) continue;
		for(j = 0; j < t->length; j++) {
			int n;
			n = t->start_index + j;
			if(n >= t->capacity) {
				n -= t->capacity;
			}
			if(first_pt) {
				cairo_move_to(
					cr, 
					x_m * t->x_data[n] + x_b,
					y_m * t->y_data[n] + y_b
				);
				first_pt = 0;
			}
			else {
				cairo_line_to(
					cr,
					x_m * t->x_data[n] + x_b, 
					y_m * t->y_data[n] + y_b
				);
			}
		}
		cairo_stroke(cr);
	}

	// now draw the trace markers (if requested)
	for(i = 0; i < p->num_traces; i++) {
		trace_t *t = p->traces[i];
		if(t->marker_type == MARKER_NONE) {
			continue;
		}
		cairo_set_source_rgb (cr, t->marker_color.red, t->marker_color.green, t->marker_color.blue);
		if(t->length <= 0) continue;
		for(j = 0; j < t->length; j++) {
			int n;
			n = t->start_index + j;
			if(n >= t->capacity) {
				n -= t->capacity;
			}
			if(t->x_data[n] < x_axis->min_val ||
			   t->x_data[n] > x_axis->max_val ||
			   t->y_data[n] < y_axis->min_val || 
			   t->y_data[n] > y_axis->max_val
			) {
				continue;
			}
			cairo_move_to(cr, x_m * t->x_data[n] + x_b,	y_m * t->y_data[n] + y_b);
			draw_marker(cr, t->marker_type, t->marker_size);
		}
		cairo_stroke(cr);
	}
	cairo_restore(cr);

	// draw the plot area border
	if(pa->do_show_bounding_box) {
		cairo_set_source_rgb (cr, pa->border_color.red, pa->border_color.green, pa->border_color.blue);
		cairo_set_line_width(cr, pa->bounding_box_width);
		cairo_rectangle(
			cr, 
			plot_area_left_edge, 
			plot_area_top_edge,
			plot_area_right_edge - plot_area_left_edge,
			plot_area_bottom_edge - plot_area_top_edge
		);
		cairo_stroke(cr);	
	}

	// draw the zoom box if zooming is active
	if(priv->zooming) {
		double dashes[] = {4.0,4.0};
		cairo_save(cr);
		cairo_set_source_rgb (cr, 0.5, 0.5, 0.5);
		cairo_set_line_width (cr, 1.0);
		cairo_set_dash(cr, dashes, 2, 0);
		cairo_rectangle(
			cr, 
			priv->drag_start_x, 
			priv->drag_start_y,
			priv->drag_end_x - priv->drag_start_x,
			priv->drag_end_y - priv->drag_start_y
		);
		cairo_stroke(cr);	
		cairo_restore(cr);
	}
	// draw crosshair if active
	if(priv->do_show_cross_hair) {
		gint x,y;
		gtk_widget_get_pointer(plot, &x, &y);
		if(x < priv->plot.plot_area.left_edge) {
			x = priv->plot.plot_area.left_edge;
		}
		if(x > priv->plot.plot_area.right_edge) {
			x = priv->plot.plot_area.right_edge;
		}
		if(y < priv->plot.plot_area.top_edge) {
			y = priv->plot.plot_area.top_edge;
		}
		if(y > priv->plot.plot_area.bottom_edge) {
			y = priv->plot.plot_area.bottom_edge;
		}
		cairo_save(cr);
		cairo_set_source_rgb (cr, 1.0, 1.0, 0.0);
		cairo_set_line_width (cr, 1.0);

		cairo_move_to(cr, priv->plot.plot_area.left_edge, y);
		cairo_line_to(cr, x-10, y);
		cairo_move_to(cr, x-5, y);
		cairo_line_to(cr, x+5, y);
		cairo_move_to(cr, x+10, y);
		cairo_line_to(cr, priv->plot.plot_area.right_edge, y);

		cairo_move_to(cr, x, priv->plot.plot_area.top_edge);
		cairo_line_to(cr, x, y-10);
		cairo_move_to(cr, x, y-5);
		cairo_line_to(cr, x, y+5);
		cairo_move_to(cr, x, y+10);
		cairo_line_to(cr, x, priv->plot.plot_area.bottom_edge);
		cairo_stroke(cr);	
		cairo_restore(cr);
	}

	// draw coordinates if active (whether zooming or not)
	if(priv->do_show_coords) {
		gint x,y;
		gtk_widget_get_pointer(plot, &x, &y);
		if(x >= priv->plot.plot_area.left_edge &&
		   x <= priv->plot.plot_area.right_edge &&
		   y >= priv->plot.plot_area.top_edge &&
		   y <= priv->plot.plot_area.bottom_edge
		  ) {
			char x_str[100], y_str[100];
			double x_w, x_h, y_w, y_h;
			double box_w, box_h;
			sprintf(x_str, "%g", ((float)x-x_b)/x_m);
			sprintf(y_str, "%g", ((float)y-y_b)/y_m);
			x_w = get_text_width(cr, x_str, 10);
			y_w = get_text_width(cr, y_str, 10);
			x_h = get_text_height(cr, x_str, 10);
			y_h = get_text_height(cr, y_str, 10);
			box_w = 10 + ((x_w > y_w) ? x_w : y_w);
			box_h = 10 + (x_h + y_h);

			cairo_save(cr);
			// draw white rectangle
			cairo_set_source_rgb (cr, 1, 1, 1);
			cairo_rectangle(cr, x-box_w-3, y-box_h-3, box_w, box_h);
			cairo_fill_preserve(cr);
			cairo_set_source_rgb (cr, 0, 0, 0);
			cairo_set_line_width (cr, 1.0);
			cairo_stroke(cr);

			// draw coordinates...
			cairo_set_source_rgb (cr, 0, 0, 0);
			draw_horiz_text_at_point(plot, cr, x_str, x-box_w+5-3, y-5-y_h-3-2, ANCHOR_BOTTOM_LEFT);
			draw_horiz_text_at_point(plot, cr, y_str, x-box_w+5-3, y-5-3+2, ANCHOR_BOTTOM_LEFT);
			cairo_restore(cr);
		}
			
	}
		
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
	if(a->do_autoscale && a->do_loose_fit) {
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
int jbplot_set_x_axis_gridline_visible(jbplot *plot, gboolean visible) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	if(visible) {
		priv->plot.x_axis.do_show_major_gridlines = 1;	
	}
	else {
		priv->plot.x_axis.do_show_major_gridlines = 0;
	}
	gtk_widget_queue_draw((GtkWidget *)plot);
	return 0;
}

int jbplot_set_x_axis_gridline_props(jbplot *plot, line_type_t type, float width, rgb_color_t color) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	priv->plot.x_axis.major_gridline_width = width;	
	priv->plot.x_axis.major_gridline_color = color;	
	priv->plot.x_axis.major_gridline_type = type;
	gtk_widget_queue_draw((GtkWidget *)plot);
	return 0;
}

int jbplot_set_bg_color(jbplot *plot, rgb_color_t color) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	priv->plot.bg_color = color;
	gtk_widget_queue_draw((GtkWidget *)plot);
	return 0;
}

int jbplot_set_plot_area_color(jbplot *plot, rgb_color_t color) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	priv->plot.plot_area.bg_color = color;
	gtk_widget_queue_draw((GtkWidget *)plot);
	return 0;
}

int jbplot_set_plot_area_border(jbplot *plot, float width, rgb_color_t color) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	priv->plot.plot_area.bounding_box_width = width;
	priv->plot.plot_area.border_color = color;
	if(width > 0) {
		priv->plot.plot_area.do_show_bounding_box = 1;
	}
	else {
		priv->plot.plot_area.do_show_bounding_box = 0;
	}
	gtk_widget_queue_draw((GtkWidget *)plot);
	return 0;
}

int jbplot_set_plot_area_margins(jbplot *plot, float left, float right, float top, float bottom) {
	// TODO
	return 0;
}


int jbplot_set_x_axis_scale_mode(jbplot *plot, scale_mode_t mode) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	if(mode == SCALE_AUTO_TIGHT) {
		priv->plot.x_axis.do_autoscale = 1;
		priv->plot.x_axis.do_loose_fit = 0;
	}
	else if(mode == SCALE_AUTO_LOOSE) {
		priv->plot.x_axis.do_autoscale = 1;
		priv->plot.x_axis.do_loose_fit = 1;
	}
	else if(mode == SCALE_MANUAL) {
		priv->plot.x_axis.do_autoscale = 0;
		priv->plot.x_axis.do_loose_fit = 0;
	}

	return 0;
}

int jbplot_set_y_axis_scale_mode(jbplot *plot, scale_mode_t mode) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	if(mode == SCALE_AUTO_TIGHT) {
		priv->plot.y_axis.do_autoscale = 1;
		priv->plot.y_axis.do_loose_fit = 0;
	}
	else if(mode == SCALE_AUTO_LOOSE) {
		priv->plot.y_axis.do_autoscale = 1;
		priv->plot.y_axis.do_loose_fit = 1;
	}
	else if(mode == SCALE_MANUAL) {
		priv->plot.y_axis.do_autoscale = 0;
		priv->plot.y_axis.do_loose_fit = 0;
	}

	return 0;
}

int jbplot_set_x_axis_range(jbplot *plot, float min, float max) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	priv->plot.x_axis.do_autoscale = 0;
	priv->plot.x_axis.min_val = min;
	priv->plot.x_axis.max_val = max;
	//printf("Setting x-axis range to (%g , %g)\n", min, max);
	gtk_widget_queue_draw((GtkWidget *)plot);
	return 0;
}
	
int jbplot_set_y_axis_range(jbplot *plot, float min, float max) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	priv->plot.y_axis.do_autoscale = 0;
	priv->plot.y_axis.min_val = min;
	priv->plot.y_axis.max_val = max;
	//printf("Setting y-axis range to (%g , %g)\n", min, max);
	gtk_widget_queue_draw((GtkWidget *)plot);
	return 0;
}
	
void jbplot_refresh(jbplot *plot) {
	gtk_widget_queue_draw((GtkWidget *)plot);
	return;
}

int jbplot_trace_set_line_props(trace_t *t, line_type_t type, float width, rgb_color_t color) {
	t->line_type = type;
	t->line_width = width;
	t->line_color = color;
	return 0;
}

int jbplot_trace_set_marker_props(trace_t *t, marker_type_t type, float size, rgb_color_t color) {
	t->marker_type = type;
	t->marker_size = size;
	t->marker_color = color;
	return 0;
}


int jbplot_trace_add_point(trace_t *t, float x, float y) {
	t->length++;
	t->end_index++;
	if(t->end_index >= t->capacity) {
		t->end_index = 0;
	}
	if(t->end_index == t->start_index) {
		t->start_index++;
		t->length = t->capacity;
	}
	if(t->start_index >= t->capacity) {
		t->start_index = 0;
	}
	*(t->x_data + t->end_index) = x;
	*(t->y_data + t->end_index) = y;
	return 0;
}

trace_t *jbplot_create_trace(int capacity) {
	trace_t *t;
	t = malloc(sizeof(trace_t));
	if(t==NULL) {
		return NULL;
	}	
	t->x_data = malloc(sizeof(float)*capacity);
	if(t->x_data==NULL) {
		free(t);
		return NULL;
	}
	t->y_data = malloc(sizeof(float)*capacity);
	if(t->y_data==NULL) {
		free(t);
		free(t->x_data);
		return NULL;
	}
	t->is_data_owner = 1;
	t->start_index = 0;
	t->end_index = 0;
	t->length = 0;
	t->capacity = capacity;
	t->line_width = 1.0;
	t->line_type = LINETYPE_SOLID;
	t->marker_type = MARKER_NONE;
	t->line_color.red = 0.0;
	t->line_color.green = 0.0;
	t->line_color.blue = 0.0;
	t->marker_color.red = 0.0;
	t->marker_color.green = 0.0;
	t->marker_color.blue = 0.0;

	return t;
}

void jbplot_destroy_trace(trace_t *trace) {
	if(trace->is_data_owner) {
		free(trace->x_data);
		free(trace->y_data);
	}
	free(trace);
	return;
}


int jbplot_add_trace(jbplot *plot, trace_t *t) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	plot_t *p = &(priv->plot);

	if(p->num_traces + 1 >= MAX_NUM_TRACES) {
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

int jbplot_set_plot_title_visible(jbplot *plot, gboolean visible) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	if(visible) {
		priv->plot.do_show_plot_title = 1;
	}
	else {
		priv->plot.do_show_plot_title = 0;
	}
	gtk_widget_queue_draw((GtkWidget *)plot);
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

int jbplot_set_x_axis_label_visible(jbplot *plot, gboolean visible) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	if(visible) {
		priv->plot.x_axis.do_show_axis_label = 1;
	}
	else {
		priv->plot.x_axis.do_show_axis_label = 0;
	}
	gtk_widget_queue_draw((GtkWidget *)plot);
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

int jbplot_set_y_axis_label_visible(jbplot *plot, gboolean visible) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	if(visible) {
		priv->plot.y_axis.do_show_axis_label = 1;
	}
	else {
		priv->plot.y_axis.do_show_axis_label = 0;
	}
	gtk_widget_queue_draw((GtkWidget *)plot);
	return 0;
}
	
