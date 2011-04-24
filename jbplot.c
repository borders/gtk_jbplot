/*
 * jbplot.c
 *
 * A GTK+ widget that implements a plot
 *
 * Author:
 *   James Borders
 * 
*/

#include <gtk/gtk.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <cairo/cairo-svg.h>

#include "jbplot.h"
#include "jbplot-marshallers.h"


#define JBPLOT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), JBPLOT_TYPE, jbplotPrivate))

G_DEFINE_TYPE (jbplot, jbplot, GTK_TYPE_DRAWING_AREA);

static gboolean jbplot_expose (GtkWidget *plot, GdkEventExpose *event);
static gboolean jbplot_configure (GtkWidget *plot, GdkEventConfigure *event);

#define MAX_NUM_MAJOR_TICS    50
#define MAJOR_TIC_LABEL_SIZE  150
#define MAX_NUM_TRACES    100

#define MED_GAP 6


double dash_pattern[] = {4.0, 4.0};
double dot_pattern[] =  {2.0, 4.0};

typedef struct box_size_t {
  double width;
  double height;
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
  double major_gridline_width;
	rgb_color_t major_gridline_color;
	int major_gridline_type;
  char do_show_minor_gridlines;
  double major_tic_values[MAX_NUM_MAJOR_TICS];
  char *major_tic_labels[MAX_NUM_MAJOR_TICS];
  char is_tic_label_owner;
  double min_val;
  double max_val;
  double major_tic_delta;
  char tic_label_format_string[100];
  char coord_label_format_string[100];
	char do_auto_tic_format;
  double tic_label_font_size;
  int num_actual_major_tics;
  int num_request_major_tics;
  int num_minor_tics_per_major;
	char do_manual_tics;
} axis_t;

typedef struct data_range {
  double min;
  double max;
} data_range;

typedef struct plot_area_t {
  char do_show_bounding_box;
  double bounding_box_width;
	double left_edge;
	double right_edge;
	double ideal_left_margin;
	double ideal_right_margin;
	double top_edge;
	double bottom_edge;
	margin_mode_t LR_margin_mode;
	double lmargin;
	double rmargin;
	rgb_color_t bg_color;
	rgb_color_t border_color;
} plot_area_t;

typedef struct legend_t {
  char do_show_bounding_box;
  double bounding_box_width;
	rgb_color_t bg_color;
	rgb_color_t border_color;
	legend_pos_t position;
	double font_size;
	char needs_redraw;
	box_size_t size;
} legend_t;

#define MAX_TRACE_NAME_LENGTH 255
typedef struct trace_t {
  double *x_data;
  double *y_data;
  int capacity;
  int length;
  int start_index;
  int end_index;
  int is_data_owner;
	double line_width;
	int line_type;
	rgb_color_t line_color;
	rgb_color_t marker_color;
	int marker_type;
	double marker_size;
	char name[MAX_TRACE_NAME_LENGTH + 1];
	int decimate_divisor;
	int lossless_decimation;
} trace_t;

typedef struct cursor_t {
	int type;
	rgb_color_t color;
	double line_width;
	int line_type;
	double x;
	double y;
} cursor_t;

typedef struct plot_t {
	rgb_color_t bg_color;
  struct plot_area_t plot_area;
  struct legend_t legend;
  struct axis_t x_axis;
  struct axis_t y_axis;
  char do_show_plot_title;
  char *plot_title;
  char is_plot_title_owner;
  double plot_title_font_size;
  struct box_size_t plot_title_size;  
  
  struct trace_t *traces[MAX_NUM_TRACES];
  int num_traces;

	cursor_t cursor;
} plot_t;


/* private (static) plotting utility functions */
static double get_text_height(cairo_t *cr, char *text, double font_size);
static double get_text_width(cairo_t *cr, char *text, double font_size);
static int set_major_tic_values(axis_t *a, double min, double max);
static int set_major_tic_labels(axis_t *a);
static double get_widest_label_width(axis_t *a, cairo_t *cr);
static void get_double_parts(double f, double *mantissa, int *exponent);
static double round_to_nearest(double num, double nearest);
static double round_up_to_nearest(double num, double nearest);
static double round_down_to_nearest(double num, double nearest);
static data_range get_y_range(trace_t **traces, int num_traces);
static data_range get_y_range_within_x_range(trace_t **traces, int num_traces, data_range xr);
static data_range get_x_range(trace_t **traces, int num_traces);
static data_range get_x_range_within_y_range(trace_t **traces, int num_traces, data_range yr);


typedef struct _jbplotPrivate jbplotPrivate;

struct _jbplotPrivate
{
	plot_t plot;
	gboolean zooming; 
	gboolean h_zoom; 
	gboolean v_zoom; 
	gboolean panning; 
	gboolean do_show_coords;
	gboolean do_show_cross_hair;
	gboolean do_snap_to_data;
	gboolean cross_hair_is_visible;
	gdouble drag_start_x;
	gdouble drag_start_y;
	gdouble drag_end_x;
	gdouble drag_end_y;
	gdouble pan_start_x;
	gdouble pan_start_y;
	data_range pan_start_x_range;
	data_range pan_start_y_range;
	GTimeVal last_mouse_motion;
	gboolean rt_mode;
	gboolean needs_redraw;
	gboolean needs_h_zoom_signal;
	gboolean needs_v_zoom_signal;
	gboolean get_ideal_lr;

	/* antialias mode */
	char antialias;

	/* these are used to convert from device coords (pixels) to data coords */
	double x_m;
	double x_b;
	double y_m;
	double y_b;

	/* image buffers used for non real-time mode */
	cairo_surface_t *legend_buffer;
	cairo_t *legend_context;
	cairo_surface_t *plot_buffer;
	cairo_t *plot_context;
	
};

enum
{
	ZOOM_IN,
	ZOOM_ALL,
	PAN,
	LAST_SIGNAL
};

static guint jbplot_signals[LAST_SIGNAL] = { 0 };


static gboolean popup_responder(GtkWidget *w, GdkEvent *e, gpointer data) {
	printf("Action not yet implemented (%s)\n", (char *)data);
	return FALSE;
}

static gboolean popup_callback_snap_to_data(GtkWidget *w, GdkEvent *e, gpointer data) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE((jbplot *) data);
	printf("Toggling snap_to_data state\n");
	priv->do_snap_to_data = !(priv->do_snap_to_data);
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


static gboolean popup_callback_zoom_all(GtkWidget *w, GdkEvent *e, gpointer data) {
	jbplot_set_x_axis_scale_mode((jbplot*)data, SCALE_AUTO_TIGHT);
	jbplot_set_y_axis_scale_mode((jbplot*)data, SCALE_AUTO_TIGHT);
	g_signal_emit_by_name((gpointer *)data, "zoom-all");
	return FALSE;
}

static gboolean popup_callback_x_autoscale(GtkWidget *w, GdkEvent *e, gpointer data) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE((jbplot *) data);
	//printf("Toggling x-axis autoscale state\n");
	priv->plot.x_axis.do_autoscale = !(priv->plot.x_axis.do_autoscale);
	priv->needs_redraw = TRUE;
	gtk_widget_queue_draw((GtkWidget *)data);
	if(priv->plot.x_axis.do_autoscale) {
		g_signal_emit_by_name((gpointer *)data, "zoom-all");
	}
	return FALSE;
}

static gboolean popup_callback_x_loose_fit(GtkWidget *w, GdkEvent *e, gpointer data) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE((jbplot *) data);
	//printf("Toggling x-axis loose fit state\n");
	priv->plot.x_axis.do_loose_fit = !(priv->plot.x_axis.do_loose_fit);
	priv->needs_redraw = TRUE;
	gtk_widget_queue_draw((GtkWidget *)data);
	return FALSE;
}

static gboolean popup_callback_y_autoscale(GtkWidget *w, GdkEvent *e, gpointer data) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE((jbplot *) data);
	//printf("Toggling y-axis autoscale state\n");
	priv->plot.y_axis.do_autoscale = !(priv->plot.y_axis.do_autoscale);
	priv->needs_redraw = TRUE;
	gtk_widget_queue_draw((GtkWidget *)data);
	if(priv->plot.y_axis.do_autoscale) {
		g_signal_emit_by_name((gpointer *)data, "zoom-all");
	}
	return FALSE;
}

static gboolean popup_callback_y_loose_fit(GtkWidget *w, GdkEvent *e, gpointer data) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE((jbplot *) data);
	//printf("Toggling y-axis loose fit state\n");
	priv->plot.y_axis.do_loose_fit = !(priv->plot.y_axis.do_loose_fit);
	priv->needs_redraw = TRUE;
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


	GtkWidget *zoom_all = gtk_menu_item_new_with_label("Zoom All");
	GtkWidget *x = gtk_menu_item_new_with_label("x-axis");
	GtkWidget *y = gtk_menu_item_new_with_label("y-axis");
	GtkWidget *show_coords = gtk_check_menu_item_new_with_label("Show Coords");
	GtkWidget *show_cross_hair = gtk_check_menu_item_new_with_label("Show Crosshair");
	GtkWidget *snap_to_data = gtk_check_menu_item_new_with_label("Snap to Data");

	if(priv->do_show_coords) {
		gtk_check_menu_item_set_active((GtkCheckMenuItem *)show_coords, TRUE);
	}
	if(priv->do_show_cross_hair) {
		gtk_check_menu_item_set_active((GtkCheckMenuItem *)show_cross_hair, TRUE);
	}
	if(priv->do_snap_to_data) {
		gtk_check_menu_item_set_active((GtkCheckMenuItem *)snap_to_data, TRUE);
	}

	g_signal_connect(G_OBJECT(zoom_all), "button-press-event", G_CALLBACK(popup_callback_zoom_all), (gpointer) my_widget);
	g_signal_connect(G_OBJECT(x), "button-press-event", G_CALLBACK(popup_responder), (gpointer) "x-axis");
	g_signal_connect(G_OBJECT(y), "button-press-event", G_CALLBACK(popup_responder), (gpointer) "y-axis");
	g_signal_connect(G_OBJECT(show_coords), "button-press-event", G_CALLBACK(popup_callback_show_coords), (gpointer) my_widget);
	g_signal_connect(G_OBJECT(show_cross_hair), "button-press-event", G_CALLBACK(popup_callback_show_cross_hair), (gpointer) my_widget);
	g_signal_connect(G_OBJECT(snap_to_data), "button-press-event", G_CALLBACK(popup_callback_snap_to_data), (gpointer) my_widget);

	gtk_menu_shell_append((GtkMenuShell *)menu, zoom_all);
	gtk_menu_shell_append((GtkMenuShell *)menu, x);
	gtk_menu_shell_append((GtkMenuShell *)menu, y);
	gtk_menu_shell_append((GtkMenuShell *)menu, show_coords);
	gtk_menu_shell_append((GtkMenuShell *)menu, show_cross_hair);
	gtk_menu_shell_append((GtkMenuShell *)menu, snap_to_data);

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

	gtk_widget_show(zoom_all);
	gtk_widget_show(x);
	gtk_widget_show(y);
	gtk_widget_show(show_coords);
	gtk_widget_show(show_cross_hair);
	gtk_widget_show(snap_to_data);

  if(event) {
		button = event->button;
		event_time = event->time;
	}
  else {
		button = 0;
		event_time = gtk_get_current_event_time ();
	}

	gtk_menu_attach_to_widget (GTK_MENU (menu), my_widget, NULL);
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, button, event_time);
}



static gboolean jbplot_button_press(GtkWidget *w, GdkEventButton *event) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE((jbplot*)w);
	if(event->type == GDK_2BUTTON_PRESS) { // double click
		jbplot_set_x_axis_scale_mode((jbplot*)w, SCALE_AUTO_TIGHT);
		jbplot_set_y_axis_scale_mode((jbplot*)w, SCALE_AUTO_TIGHT);
		g_signal_emit_by_name((gpointer *)w, "zoom-all");
	}
	else {
		if(event->button == 3) {
			if(priv->zooming) {
				priv->zooming = FALSE;
				gtk_widget_queue_draw(w);
			}
			else {
				do_popup_menu(w, event);
			}
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
			priv->zooming = FALSE;
			gtk_widget_queue_draw(w);
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
			if(priv->h_zoom) {
				priv->drag_start_y = priv->plot.plot_area.top_edge;
				y_now = priv->plot.plot_area.bottom_edge;
				priv->h_zoom = FALSE;

				x_min = (x_now < priv->drag_start_x) ? x_now : priv->drag_start_x;
				x_max = (x_now > priv->drag_start_x) ? x_now : priv->drag_start_x;
				y_min = (y_now < priv->drag_start_y) ? y_now : priv->drag_start_y;
				y_max = (y_now > priv->drag_start_y) ? y_now : priv->drag_start_y;
				double xmin = (x_min - priv->x_b)/priv->x_m;
				double xmax = (x_max - priv->x_b)/priv->x_m;
				double ymin = (y_max - priv->y_b)/priv->y_m;
				double ymax = (y_min - priv->y_b)/priv->y_m;
				jbplot_set_x_axis_range((jbplot *)w, xmin, xmax);
				priv->needs_redraw = TRUE;
				priv->needs_h_zoom_signal = TRUE;
				//g_signal_emit_by_name((gpointer *)w, "zoom-in", xmin, xmax, ymin, ymax);
			}
			else if(priv->v_zoom) {
				priv->drag_start_x = priv->plot.plot_area.left_edge;
				x_now = priv->plot.plot_area.right_edge;
				priv->v_zoom = FALSE;

				x_min = (x_now < priv->drag_start_x) ? x_now : priv->drag_start_x;
				x_max = (x_now > priv->drag_start_x) ? x_now : priv->drag_start_x;
				y_min = (y_now < priv->drag_start_y) ? y_now : priv->drag_start_y;
				y_max = (y_now > priv->drag_start_y) ? y_now : priv->drag_start_y;
				double xmin = (x_min - priv->x_b)/priv->x_m;
				double xmax = (x_max - priv->x_b)/priv->x_m;
				double ymin = (y_max - priv->y_b)/priv->y_m;
				double ymax = (y_min - priv->y_b)/priv->y_m;
				jbplot_set_y_axis_range((jbplot *)w, ymin, ymax);
				priv->needs_redraw = TRUE;
				priv->needs_v_zoom_signal = TRUE;
				//g_signal_emit_by_name((gpointer *)w, "zoom-in", xmin, xmax, ymin, ymax);
			}
			else if(x_now != priv->drag_start_x && y_now != priv->drag_start_y) {
				x_min = (x_now < priv->drag_start_x) ? x_now : priv->drag_start_x;
				x_max = (x_now > priv->drag_start_x) ? x_now : priv->drag_start_x;
				y_min = (y_now < priv->drag_start_y) ? y_now : priv->drag_start_y;
				y_max = (y_now > priv->drag_start_y) ? y_now : priv->drag_start_y;
				double xmin = (x_min - priv->x_b)/priv->x_m;
				double xmax = (x_max - priv->x_b)/priv->x_m;
				double ymin = (y_max - priv->y_b)/priv->y_m;
				double ymax = (y_min - priv->y_b)/priv->y_m;
				jbplot_set_x_axis_range((jbplot *)w, xmin, xmax);
				jbplot_set_y_axis_range((jbplot *)w, ymin, ymax);
				priv->needs_redraw = TRUE;
				g_signal_emit_by_name((gpointer *)w, "zoom-in", xmin, xmax, ymin, ymax);
			}
			gtk_widget_queue_draw(w);
		}
	}
	else if(event->button == 2) {
		if(priv->panning) {
			priv->panning = FALSE;
			priv->needs_redraw = TRUE;
			gtk_widget_queue_draw(w);
		}
	}
	return FALSE;
}


static gboolean jbplot_leave_notify(GtkWidget *w, GdkEventCrossing *event) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE((jbplot*)w);
	if(priv->cross_hair_is_visible) {
		priv->needs_redraw = TRUE;
		gtk_widget_queue_draw(w);
	}
	return FALSE;
}

static gboolean jbplot_motion_notify(GtkWidget *w, GdkEventMotion *event) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE((jbplot*)w);
	GTimeVal t_now;
	g_get_current_time(&t_now);
	if((t_now.tv_sec-priv->last_mouse_motion.tv_sec) + 1.e-6*(t_now.tv_usec-priv->last_mouse_motion.tv_usec) < 60.e-3) {
		return FALSE;
	}
	priv->last_mouse_motion = t_now;
	if(priv->zooming) {
		priv->h_zoom = FALSE;
		priv->v_zoom = FALSE;
		if(event->state & GDK_SHIFT_MASK) {
			priv->h_zoom = TRUE;
		}
		else if(event->state & GDK_CONTROL_MASK) {
			priv->v_zoom = TRUE;
		}
		priv->drag_end_x = event->x;
		priv->drag_end_y = event->y;
		gtk_widget_queue_draw(w);
	}
	if(priv->panning) {
		double xmin, xmax, ymin, ymax;
		xmin = priv->pan_start_x_range.min - (event->x - priv->pan_start_x)/priv->x_m;
		xmax = priv->pan_start_x_range.max - (event->x - priv->pan_start_x)/priv->x_m;
		ymin = priv->pan_start_y_range.min - (event->y - priv->pan_start_y)/priv->y_m;
		ymax = priv->pan_start_y_range.max - (event->y - priv->pan_start_y)/priv->y_m;
		jbplot_set_x_axis_range((jbplot *)w, xmin, xmax);
		jbplot_set_y_axis_range((jbplot *)w, ymin, ymax);
		priv->needs_redraw = TRUE;
		gtk_widget_queue_draw(w);
		g_signal_emit_by_name((gpointer *)w, "pan", xmin, xmax, ymin, ymax);
	}
	if(priv->do_show_coords) {
		gtk_widget_queue_draw(w);
	}
	if(priv->do_show_cross_hair) {
		gtk_widget_queue_draw(w);
	}
	return FALSE;
}


static void zoom_in (jbplot *plot, gdouble xmin, gdouble xmax, gdouble ymin, gdouble ymax) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	printf("got zoom_in event\n");
	printf("xmin = %g, xmax = %g\n", xmin, xmax);
	printf("ymin = %g, ymax = %g\n", ymin, ymax);
}

static void zoom_all (jbplot *plot) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	printf("got zoom_all event\n");
}


static void jbplot_class_init (jbplotClass *class) {
	GObjectClass *obj_class;
	GtkWidgetClass *widget_class;

	obj_class = G_OBJECT_CLASS (class);
	widget_class = GTK_WIDGET_CLASS (class);

	/* GtkWidget signals */
	widget_class->expose_event = jbplot_expose;
	widget_class->configure_event = jbplot_configure;
	widget_class->button_press_event = jbplot_button_press;
	widget_class->button_release_event = jbplot_button_release;
	widget_class->motion_notify_event = jbplot_motion_notify;
	widget_class->leave_notify_event = jbplot_leave_notify;

	/* jbplot signals */
	jbplot_signals[ZOOM_IN] = g_signal_new (
		"zoom-in",
		G_OBJECT_CLASS_TYPE(obj_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET(jbplotClass, zoom_in),
		NULL, NULL,
		_plot_marshal_VOID__DOUBLE_DOUBLE_DOUBLE_DOUBLE,
		G_TYPE_NONE, 4,
		G_TYPE_DOUBLE, G_TYPE_DOUBLE,
		G_TYPE_DOUBLE, G_TYPE_DOUBLE);

	jbplot_signals[ZOOM_ALL] = g_signal_new (
		"zoom-all",
		G_OBJECT_CLASS_TYPE(obj_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET(jbplotClass, zoom_all),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	jbplot_signals[PAN] = g_signal_new (
		"pan",
		G_OBJECT_CLASS_TYPE(obj_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET(jbplotClass, zoom_in),
		NULL, NULL,
		_plot_marshal_VOID__DOUBLE_DOUBLE_DOUBLE_DOUBLE,
		G_TYPE_NONE, 4,
		G_TYPE_DOUBLE, G_TYPE_DOUBLE,
		G_TYPE_DOUBLE, G_TYPE_DOUBLE);

	/* default handlers for signals */
	//class->zoom_in = zoom_in;
	//class->zoom_all = zoom_all;

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
  axis->major_tic_delta = 1.0;
  strcpy(axis->tic_label_format_string, "%g");
  strcpy(axis->coord_label_format_string, "%g");
	axis->do_auto_tic_format = 1;
  axis->num_request_major_tics = 8;
  axis->num_minor_tics_per_major = 5;
  axis->tic_label_font_size = 10.;
  axis->axis_label_font_size = 10.;
	axis->do_manual_tics = 0;
	return 0;
}

static int init_plot_area(plot_area_t *area) {
	rgb_color_t color = {0.0, 0.0, 0.0};
  area->do_show_bounding_box = 1;
  area->bounding_box_width = 2.0;
	area->border_color = color;
	area->LR_margin_mode = MARGIN_AUTO;
	color.red = color.green = color.blue = 1.0;
	area->bg_color = color;
	return 0;
}

static int init_legend(legend_t *legend) {
	rgb_color_t color = {0.0, 0.0, 0.0};
  legend->do_show_bounding_box = 1;
  legend->bounding_box_width = 1.0;
	legend->border_color = color;
	color.red = color.green = color.blue = 1.0;
	legend->bg_color = color;
	legend->position = LEGEND_POS_RIGHT;
	legend->font_size = 10.;
	legend->needs_redraw = 1;
	return 0;
}


static int init_plot(plot_t *plot) {
	rgb_color_t color = {1.0, 1.0, 1.0};

	if(	init_axis(&(plot->x_axis)) 
							||
			init_axis(&(plot->y_axis)) 
							||
			init_plot_area(&(plot->plot_area))
	            ||
	    init_legend(&(plot->legend))
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
			GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK);

	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);

	priv->zooming = FALSE;
	priv->h_zoom = FALSE;
	priv->v_zoom = FALSE;
	priv->panning = FALSE;
	priv->do_show_coords = FALSE;
	priv->cross_hair_is_visible = FALSE;
	priv->do_show_cross_hair = FALSE;
	priv->do_snap_to_data = FALSE;
	priv->antialias = 0;
	priv->rt_mode = TRUE;
	priv->needs_redraw = TRUE;
	priv->needs_h_zoom_signal = FALSE;
	priv->needs_v_zoom_signal = FALSE;
	priv->get_ideal_lr = FALSE;

	// initialize the plot struct elements
	init_plot(&(priv->plot));

	jbplot_set_plot_title(plot, " ", 1);
	jbplot_set_x_axis_label(plot, " ", 1);
	jbplot_set_y_axis_label(plot, " ", 1);

	g_get_current_time(&priv->last_mouse_motion);

	priv->legend_context = NULL;
	priv->legend_buffer = NULL;
	priv->plot_context = NULL;
	priv->plot_buffer = NULL;
	
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


static int draw_horiz_text_at_point(void *cr, char *text, double x, double y, anchor_t anchor) {
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


void draw_marker(cairo_t *cr, int type, double size) {
	double x, y;
	cairo_get_current_point(cr, &x, &y);
	if(type == MARKER_POINT) {
		cairo_move_to(cr, x, y);
		cairo_close_path(cr);
		cairo_stroke(cr);
	}
	else if(type == MARKER_CIRCLE) {
		cairo_arc(cr, x, y, size/2.0, 0, 2*M_PI);
		cairo_fill(cr);
	}
	else if(type == MARKER_SQUARE) {
		cairo_rectangle(cr, x-size/2.0, y-size/2.0, size, size);
		cairo_fill(cr);
	}
	return;
}

int calc_legend_dims(plot_t *plot, cairo_t *cr, double *width, double *height, double *spacing) {
	double max_width = 10.;
	double h_sum = 10.;
	int i;
		double w;
		double h;
	for(i=0; i < plot->num_traces; i++) {
		w = get_text_width(cr, plot->traces[i]->name, plot->legend.font_size);
		h = get_text_height(cr, plot->traces[i]->name, plot->legend.font_size);
		if(w > max_width) {
			max_width = w;
		}
		h_sum = h_sum + h + 10;
	}
	*width = max_width + 15 + 10;
	*height = h_sum;
	*spacing = h + 10;
	return 0;
}


static int draw_legend(jbplot *plot) {
	jbplotPrivate	*priv = JBPLOT_GET_PRIVATE(plot);
	plot_t *p = &(priv->plot);
	legend_t *l = &(p->legend);
	double border_margin = 3.;
	double line_length = 15;
	double text_to_line_gap = 5;

	if(!l->needs_redraw) {
		return 0;
	}
	l->needs_redraw = 0;

	if(priv->legend_context != NULL) {
		cairo_destroy(priv->legend_context);
	}
	if(priv->legend_buffer != NULL) {
		cairo_surface_destroy(priv->legend_buffer);
	}
	//priv->legend_buffer = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, ((GtkWidget *)plot)->allocation.width, ((GtkWidget *)plot)->allocation.height);
	priv->legend_buffer = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 2000, 2000);
	cairo_status_t stat = cairo_surface_status(priv->legend_buffer);
	if(stat != CAIRO_STATUS_SUCCESS) {
		printf("Error creating cairo image surface: %s\n", cairo_status_to_string(stat));
	}
	priv->legend_context = cairo_create(priv->legend_buffer);
	cairo_status_t cr_stat = cairo_status(priv->legend_context);
	if(cr_stat != CAIRO_STATUS_SUCCESS) {
		printf("Error creating cairo image context: %s\n", cairo_status_to_string(cr_stat));
		return FALSE;
	}
	cairo_t *cr = priv->legend_context;


	if(l->position == LEGEND_POS_RIGHT) {
		// first calculate the widest trace name
		double max_width = 10.;
		int i;
		double w;
		for(i=0; i < p->num_traces; i++) {
			/* skip traces with empty names */
			if(strlen(p->traces[i]->name) == 0) {
				continue;
			}
			w = get_text_width(cr, p->traces[i]->name, p->legend.font_size);
			if(w > max_width) {
				max_width = w;
			}
		}

		double x_start = border_margin + max_width + text_to_line_gap;
		cairo_set_font_size(cr, p->legend.font_size);
		double entry_spacing = 1.5 * get_text_height(cr, "Test", p->legend.font_size);
		int j=0;
		for(i=0; i < p->num_traces; i++) {
			/* skip traces with empty names */
			if(strlen(p->traces[i]->name) == 0) {
				continue;
			}
			cairo_set_source_rgb (cr, 0., 0., 0.);
			draw_horiz_text_at_point(	cr, 
			                          p->traces[i]->name, 
																border_margin, 
																border_margin + entry_spacing*j, 
																ANCHOR_TOP_LEFT
															);
			if(p->traces[i]->line_type != LINETYPE_NONE) {

				if(p->traces[i]->line_type == LINETYPE_SOLID) {
					cairo_set_dash(cr, dash_pattern, 0, 0);
				}	
				else if(p->traces[i]->line_type == LINETYPE_DASHED) {
					cairo_set_dash(cr, dash_pattern, 2, 0);
				}	
				else if(p->traces[i]->line_type == LINETYPE_DOTTED) {
					cairo_set_dash(cr, dot_pattern, 2, 0);
				}	

				cairo_set_source_rgb(cr, 
				                     p->traces[i]->line_color.red,
				                     p->traces[i]->line_color.green,
				                     p->traces[i]->line_color.blue
				);
				cairo_set_line_width(cr, p->traces[i]->line_width);
				double h = border_margin + entry_spacing * j + 0.5 * get_text_height(cr, p->traces[i]->name, p->legend.font_size);
				cairo_move_to(cr, x_start, h);
				cairo_line_to(cr, x_start + line_length, h);
				cairo_stroke(cr);
			}
			if(p->traces[i]->marker_type != MARKER_NONE) {
				cairo_set_source_rgb(cr, 
				                     p->traces[i]->marker_color.red,
				                     p->traces[i]->marker_color.green,
				                     p->traces[i]->marker_color.blue
				);
				cairo_set_line_width(cr, p->traces[i]->line_width);
				double h = border_margin + entry_spacing * j + 0.5 * get_text_height(cr, p->traces[i]->name, p->legend.font_size);
				cairo_move_to(cr, x_start + line_length/2., h);
				draw_marker(cr, p->traces[i]->marker_type, p->traces[i]->marker_size);
			}
			j++;
		}
		l->size.width = x_start + line_length + border_margin;
		l->size.height = border_margin + j * entry_spacing + border_margin;
		if(l->do_show_bounding_box) {
			cairo_set_dash(cr, dash_pattern, 0, 0);
			cairo_set_source_rgb(cr, l->border_color.red, l->border_color.green, l->border_color.blue);
			cairo_set_line_width(cr, l->bounding_box_width);
			cairo_move_to(cr, 0, 0);
			cairo_line_to(cr, l->size.width, 0);
			cairo_line_to(cr, l->size.width, l->size.height);
			cairo_line_to(cr, 0, l->size.height);
			cairo_line_to(cr, 0, 0);
			cairo_stroke(cr);
		}
	}
	else if(l->position == LEGEND_POS_TOP) {
		int i;
		double h_space = 10;

		// first calculate the cumulative width of the legend entries
		double total_width = 0.;
		double w;
		for(i=0; i < p->num_traces; i++) {
			/* skip traces with empty names */
			if(strlen(p->traces[i]->name) == 0) {
				continue;
			}
			w = get_text_width(cr, p->traces[i]->name, p->legend.font_size);
			total_width += w + text_to_line_gap + line_length + h_space;
		}
		if(total_width > ((GtkWidget *)plot)->allocation.width) {
			// the legend entries must span more than one line.  In this case,
			// we'll allocate an equal width of space for each legend entry

			// first calculate the widest trace name
			double max_width = 10.;
			double w;
			for(i=0; i < p->num_traces; i++) {
				/* skip traces with empty names */
				if(strlen(p->traces[i]->name) == 0) {
					continue;
				}
				w = get_text_width(cr, p->traces[i]->name, p->legend.font_size);
				if(w > max_width) {
					max_width = w;
				}
			}
			double entry_width = max_width + text_to_line_gap + line_length + h_space;
		}
		else {
			// all the legend entries will fit on a single line
			double x = border_margin;
			cairo_set_font_size(cr, p->legend.font_size);
			for(i=0; i < p->num_traces; i++) {
				/* skip traces with empty names */
				if(strlen(p->traces[i]->name) == 0) {
					continue;
				}
				cairo_set_source_rgb (cr, 0., 0., 0.);
				draw_horiz_text_at_point(	cr, 
																	p->traces[i]->name, 
																	x, 
																	border_margin, 
																	ANCHOR_TOP_LEFT
																);
				x += get_text_width(cr, p->traces[i]->name, p->legend.font_size) + text_to_line_gap;
				if(p->traces[i]->line_type != LINETYPE_NONE) {
					cairo_set_source_rgb(cr, 
															 p->traces[i]->line_color.red,
															 p->traces[i]->line_color.green,
															 p->traces[i]->line_color.blue
					);
					cairo_set_line_width(cr, p->traces[i]->line_width);
					double h = border_margin + 0.5 * get_text_height(cr, p->traces[i]->name, p->legend.font_size);
					cairo_move_to(cr, x, h);
					cairo_line_to(cr, x + line_length, h);
					cairo_stroke(cr);
				}
				if(p->traces[i]->marker_type != MARKER_NONE) {
					cairo_set_source_rgb(cr, 
															 p->traces[i]->marker_color.red,
															 p->traces[i]->marker_color.green,
															 p->traces[i]->marker_color.blue
					);
					cairo_set_line_width(cr, p->traces[i]->line_width);
					double h = border_margin + 0.5 * get_text_height(cr, p->traces[i]->name, p->legend.font_size);
					cairo_move_to(cr, x + line_length/2., h);
					draw_marker(cr, p->traces[i]->marker_type, p->traces[i]->marker_size);
				}
				x += line_length + h_space;
			}
			l->size.width = border_margin + total_width + border_margin;
			l->size.height = border_margin + get_text_height(cr, "Test", p->legend.font_size) + border_margin;
			if(l->do_show_bounding_box) {
				cairo_set_source_rgb(cr, l->border_color.red, l->border_color.green, l->border_color.blue);
				cairo_set_line_width(cr, l->bounding_box_width);
				cairo_move_to(cr, 0, 0);
				cairo_line_to(cr, l->size.width, 0);
				cairo_line_to(cr, l->size.width, l->size.height);
				cairo_line_to(cr, 0, l->size.height);
				cairo_line_to(cr, 0, 0);
				cairo_stroke(cr);
			}
		}

	}
	return 0;
}


static gboolean draw_plot(GtkWidget *plot, cairo_t *cr, double width, double height) {
	int i, j;
	jbplotPrivate	*priv = JBPLOT_GET_PRIVATE(plot);
	plot_t *p = &(priv->plot);
	axis_t *x_axis = &(p->x_axis);
	axis_t *y_axis = &(p->y_axis);
	plot_area_t *pa = &(p->plot_area);
	legend_t *l = &(p->legend);

	if(!priv->needs_redraw) {
		return FALSE;
	}
	priv->needs_redraw = FALSE;

	// set some default values in cairo context
	cairo_set_line_width(cr, 1.0);

	//First fill the background
	cairo_rectangle(cr, 0., 0., width, height);
	cairo_set_source_rgb (cr, p->bg_color.red, p->bg_color.green, p->bg_color.blue);
	cairo_fill(cr);

	// now do the layout calcs
  double title_top_edge = 0.01 * height;
	double title_bottom_edge;
	if(p->do_show_plot_title) {
	  title_bottom_edge = title_top_edge + get_text_height(cr, p->plot_title, p->plot_title_font_size);
	}
	else {
		title_bottom_edge = title_top_edge;
	}
  double x_label_bottom_edge = height - 0.01*height;
  double x_label_top_edge = x_label_bottom_edge - 
                             get_text_height(cr, x_axis->axis_label , x_axis->axis_label_font_size);

  // draw the plot title if desired	
	cairo_set_source_rgb (cr, 0., 0., 0.);
  if(p->do_show_plot_title) {
		cairo_save(cr);
		cairo_set_font_size(cr, p->plot_title_font_size);
		draw_horiz_text_at_point(cr, p->plot_title, 0.5*width, title_top_edge, ANCHOR_TOP_MIDDLE);
		cairo_restore(cr);
  }

	/********** Draw the legend ***********************/
	/* Draw the legend to the legend image buffer */
	draw_legend((jbplot *)plot);

	double legend_width, legend_height;
	double legend_top_edge, legend_left_edge;
	legend_width = l->size.width;
	legend_height = l->size.height;

	/* Then paint the legend image buffer to the plot surface */
	if(l->position != LEGEND_POS_NONE) {
		if(l->position == LEGEND_POS_RIGHT) {
			legend_left_edge = width - legend_width - 10;
			legend_top_edge = title_bottom_edge + 0.01 * height;
		}
		else if(l->position == LEGEND_POS_TOP) {
			legend_left_edge = (width - legend_width)/2.;
			legend_top_edge = title_bottom_edge + 10;
		}
		
		cairo_save(cr);
		cairo_set_source_surface(cr, priv->legend_buffer, legend_left_edge, legend_top_edge);
		cairo_rectangle(cr, legend_left_edge, legend_top_edge, l->size.width, l->size.height);
		cairo_clip(cr);
		cairo_paint(cr);
		cairo_restore(cr);
	}
	
	// calculate data ranges and tic labels
  data_range x_range, y_range;
	if(x_axis->do_autoscale) {
		if(y_axis->do_autoscale) {
	  	x_range = get_x_range(p->traces, p->num_traces);
		}
		else {
			data_range yr;
			yr.min = y_axis->min_val;
			yr.max = y_axis->max_val;
			x_range = get_x_range_within_y_range(p->traces, p->num_traces, yr);
		}
	}
	else {
		x_range.min = x_axis->min_val;
		x_range.max = x_axis->max_val;
	}
	if(y_axis->do_autoscale) {
		if(x_axis->do_autoscale) {
			y_range = get_y_range(p->traces, p->num_traces);
		}
		else {
			data_range xr;
			xr.min = x_axis->min_val;
			xr.max = x_axis->max_val;
			y_range = get_y_range_within_x_range(p->traces, p->num_traces, xr);
		}
	}
	else {
		y_range.min = y_axis->min_val;
		y_range.max = y_axis->max_val;
	}

	set_major_tic_values(x_axis, x_range.min, x_range.max);
	set_major_tic_values(y_axis, y_range.min, y_range.max);

	if(!x_axis->do_manual_tics) {
		set_major_tic_labels(x_axis);
	}
	if(!y_axis->do_manual_tics) {
		set_major_tic_labels(y_axis);
	}

  double max_y_label_width = get_widest_label_width(y_axis, cr);
  double y_tic_labels_left_edge;
  double y_tic_labels_right_edge;
  double plot_area_left_edge;
  double plot_area_right_edge;
	double y_label_left_edge;
	double y_label_right_edge; 

	// fire a zoom signal if needed
	if((priv->needs_h_zoom_signal || priv->needs_v_zoom_signal) && !priv->get_ideal_lr) {
		g_signal_emit_by_name((gpointer *)plot, "zoom-in", x_axis->min_val, x_axis->max_val, y_axis->min_val, y_axis->max_val);
		priv->needs_h_zoom_signal = FALSE;
		priv->needs_v_zoom_signal = FALSE;
	}


	if(priv->plot.plot_area.LR_margin_mode == MARGIN_AUTO || priv->get_ideal_lr) {
		y_label_left_edge = MED_GAP;
		y_label_right_edge = y_label_left_edge + 
    	get_text_height(cr, y_axis->axis_label, y_axis->axis_label_font_size);
		if(y_axis->do_show_axis_label) {
			y_tic_labels_left_edge = y_label_right_edge + MED_GAP;
		}
		else {
			y_tic_labels_left_edge = MED_GAP;
		}

		y_tic_labels_right_edge = y_tic_labels_left_edge + max_y_label_width;
		plot_area_left_edge = y_tic_labels_right_edge + MED_GAP;

		if(l->position == LEGEND_POS_RIGHT) {
			plot_area_right_edge = legend_left_edge - 10;
		}
		else {
			plot_area_right_edge = width - 0.06 * width;
		}

		double right_side_x_tic_label_width = 
			get_text_width(cr, 
			x_axis->major_tic_labels[x_axis->num_actual_major_tics-1], 
			x_axis->tic_label_font_size);
		if(0.5*right_side_x_tic_label_width > (width - plot_area_right_edge)) {
			plot_area_right_edge = width - right_side_x_tic_label_width;
		}
		priv->plot.plot_area.left_edge = plot_area_left_edge;
		priv->plot.plot_area.ideal_left_margin = plot_area_left_edge;
		priv->plot.plot_area.right_edge = plot_area_right_edge;
		priv->plot.plot_area.ideal_right_margin = width - plot_area_right_edge;
	}
	if(priv->get_ideal_lr) {
		return FALSE;
	}
	if(priv->plot.plot_area.LR_margin_mode != MARGIN_AUTO) {
		if(priv->plot.plot_area.LR_margin_mode == MARGIN_PERCENT) {
			plot_area_left_edge = priv->plot.plot_area.lmargin * width;
			plot_area_right_edge = width - priv->plot.plot_area.rmargin * width;
		}
		else { // pixels
			plot_area_left_edge = priv->plot.plot_area.lmargin;
			plot_area_right_edge = width - priv->plot.plot_area.rmargin;
		}
		priv->plot.plot_area.left_edge = plot_area_left_edge;
		priv->plot.plot_area.right_edge = plot_area_right_edge;
		y_tic_labels_right_edge = plot_area_left_edge - MED_GAP;
		y_tic_labels_left_edge = y_tic_labels_right_edge - max_y_label_width;
		y_label_right_edge = y_tic_labels_left_edge - MED_GAP;
		y_label_left_edge = y_label_right_edge - 
			get_text_height(cr, y_axis->axis_label, y_axis->axis_label_font_size);
	}

	double plot_area_top_edge;
	if(l->position == LEGEND_POS_TOP) {
		plot_area_top_edge = legend_top_edge + legend_height + 2*MED_GAP;
	}
	else {
		plot_area_top_edge = title_bottom_edge + 2*MED_GAP;
	}
	priv->plot.plot_area.top_edge = plot_area_top_edge;
  double x_tic_labels_bottom_edge;
	if(x_axis->do_show_axis_label) {
		x_tic_labels_bottom_edge = x_label_top_edge - MED_GAP;
	}
	else {
		x_tic_labels_bottom_edge = height - MED_GAP;
	}

  double x_tic_labels_height = get_text_height(cr, x_axis->major_tic_labels[0], x_axis->tic_label_font_size);
  double x_tic_labels_top_edge = x_tic_labels_bottom_edge - x_tic_labels_height;
  double plot_area_bottom_edge = x_tic_labels_top_edge - MED_GAP;
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
	if(y_axis->do_manual_tics) {
		for(i=0; i<y_axis->num_actual_major_tics; i++) {
			double val = y_axis->major_tic_values[i];
			if(val <= y_axis->max_val && val >= y_axis->min_val) {
				draw_horiz_text_at_point(	cr, 
																	y_axis->major_tic_labels[i], 
																	y_tic_labels_right_edge, 
																	y_m * y_axis->major_tic_values[i] + y_b, 
																	ANCHOR_MIDDLE_RIGHT
																);
			}
		}
	}
	else {
		for(i=0; i<y_axis->num_actual_major_tics; i++) {
			draw_horiz_text_at_point(	cr, 
																y_axis->major_tic_labels[i], 
																y_tic_labels_right_edge, 
																y_m * y_axis->major_tic_values[i] + y_b, 
																ANCHOR_MIDDLE_RIGHT
															);
		}
	}
	cairo_restore(cr);

	// draw the y major gridlines
	if(y_axis->do_show_major_gridlines && y_axis->major_gridline_type != LINETYPE_NONE) {
		cairo_save(cr);
		if(y_axis->major_gridline_type == LINETYPE_SOLID) {
			cairo_set_dash(cr, dash_pattern, 0, 0);
		}	
		else if(y_axis->major_gridline_type == LINETYPE_DASHED) {
			cairo_set_dash(cr, dash_pattern, 2, 0);
		}	
		else if(y_axis->major_gridline_type == LINETYPE_DOTTED) {
			cairo_set_dash(cr, dot_pattern, 2, 0);
		}	
		cairo_set_source_rgb(cr, 
		                     y_axis->major_gridline_color.red, 
		                     y_axis->major_gridline_color.green, 
		                     y_axis->major_gridline_color.blue
		);
		cairo_set_line_width(cr, y_axis->major_gridline_width);
		if(y_axis->do_manual_tics) {
			for(i=0; i<y_axis->num_actual_major_tics; i++) {
				double val = y_axis->major_tic_values[i];
				if(val <= y_axis->max_val && val >= y_axis->min_val) {
					double y = y_m * y_axis->major_tic_values[i] + y_b;
					cairo_move_to(cr, plot_area_left_edge, y);
					cairo_line_to(cr, plot_area_right_edge, y);
					cairo_stroke(cr);
				}
			}
		}	
		else {
			for(i=0; i<y_axis->num_actual_major_tics; i++) {
				double y = y_m * y_axis->major_tic_values[i] + y_b;
				cairo_move_to(cr, plot_area_left_edge, y);
				cairo_line_to(cr, plot_area_right_edge, y);
				cairo_stroke(cr);
			}
		}
		cairo_restore(cr);
	}
	
	// draw the x tic labels
	cairo_set_source_rgb (cr, 0., 0., 0.);
	cairo_save(cr);
	cairo_set_font_size(cr, x_axis->tic_label_font_size);
	if(x_axis->do_manual_tics) {
		for(i=0; i<x_axis->num_actual_major_tics; i++) {
			double val = x_axis->major_tic_values[i];
			if(val <= x_axis->max_val && val >= x_axis->min_val) {
				draw_horiz_text_at_point(	cr, 
																	x_axis->major_tic_labels[i], 
																	x_m * val + x_b, 
																	x_tic_labels_top_edge, 
																	ANCHOR_TOP_MIDDLE
																);
			}
		}
	}
	else {
		for(i=0; i<x_axis->num_actual_major_tics; i++) {
			draw_horiz_text_at_point(	cr, 
																x_axis->major_tic_labels[i], 
																x_m * x_axis->major_tic_values[i] + x_b, 
																x_tic_labels_top_edge, 
																ANCHOR_TOP_MIDDLE
															);
		}
	}
	cairo_restore(cr);

	// draw the x major gridlines
	if(x_axis->do_show_major_gridlines && x_axis->major_gridline_type != LINETYPE_NONE) {
		cairo_save(cr);
		if(x_axis->major_gridline_type == LINETYPE_SOLID) {
			cairo_set_dash(cr, dash_pattern, 0, 0);
		}	
		else if(x_axis->major_gridline_type == LINETYPE_DASHED) {
			cairo_set_dash(cr, dash_pattern, 2, 0);
		}	
		else if(x_axis->major_gridline_type == LINETYPE_DOTTED) {
			cairo_set_dash(cr, dot_pattern, 2, 0);
		}	
		cairo_set_source_rgb(cr, 
		                     x_axis->major_gridline_color.red, 
		                     x_axis->major_gridline_color.green, 
		                     x_axis->major_gridline_color.blue
		);
		cairo_set_line_width(cr, x_axis->major_gridline_width);
		if(x_axis->do_manual_tics) {
			for(i=0; i<x_axis->num_actual_major_tics; i++) {
				double val = x_axis->major_tic_values[i];
				if(val <= x_axis->max_val && val >= x_axis->min_val) {
					double x = x_m * val + x_b;		
					cairo_move_to(cr, x, plot_area_bottom_edge);
					cairo_line_to(cr, x, plot_area_top_edge);
					cairo_stroke(cr);
				}
			}
		}
		else {
			for(i=0; i<x_axis->num_actual_major_tics; i++) {
				double x = x_m * x_axis->major_tic_values[i] + x_b;		
				cairo_move_to(cr, x, plot_area_bottom_edge);
				cairo_line_to(cr, x, plot_area_top_edge);
				cairo_stroke(cr);
			}
		}
		cairo_restore(cr);
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
		draw_horiz_text_at_point(	cr, 
															x_axis->axis_label, 
															x_label_middle_x, 
															x_label_top_edge, 
															ANCHOR_TOP_MIDDLE
														);
	}
	

	/*********** draw the plot area border ******************/
	if(pa->do_show_bounding_box) {
		cairo_set_source_rgb (cr, pa->border_color.red, pa->border_color.green, pa->border_color.blue);
		cairo_set_line_width(cr, pa->bounding_box_width);
		cairo_rectangle(
			cr, 
			plot_area_left_edge - pa->bounding_box_width, 
			plot_area_top_edge - pa->bounding_box_width,
			plot_area_right_edge - plot_area_left_edge + 2*pa->bounding_box_width,
			plot_area_bottom_edge - plot_area_top_edge + 2*pa->bounding_box_width
		);
		cairo_stroke(cr);	
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
		char last_was_NAN = 0;
		char last_was_out = 0;
		trace_t *t = p->traces[i];
		if(t->line_type == LINETYPE_NONE) {
			continue;
		}
		cairo_set_source_rgb (cr, t->line_color.red, t->line_color.green, t->line_color.blue);
		cairo_set_line_width(cr, t->line_width);
		if(t->line_type == LINETYPE_SOLID) {
			cairo_set_dash(cr, dash_pattern, 0, 0);
		}	
		else if(t->line_type == LINETYPE_DASHED) {
			cairo_set_dash(cr, dash_pattern, 2, 0);
		}	
		else if(t->line_type == LINETYPE_DOTTED) {
			cairo_set_dash(cr, dot_pattern, 2, 0);
		}	
		if(t->length <= 0) continue;
		int dd = t->decimate_divisor;
		if(t->lossless_decimation) {
			for(j = 0; j < t->length; j += dd) {
				int last_x_px, last_y_px;
				double min_y, max_y;
				int n = t->start_index + j;
				if(n >= t->capacity) {
					n -= t->capacity;
				}
				double x_px = x_m * t->x_data[n] + x_b;
				double y_px = y_m * t->y_data[n] + y_b;
				if(first_pt) {
					cairo_move_to(cr,	x_px,	y_px);
					first_pt = 0;
					min_y = max_y = y_px;
				}
				else {
					if(x_px != last_x_px) {
						/* first draw vertical line spanning min to max for previous x-pixel */
						cairo_move_to(cr,	last_x_px,	min_y);
						cairo_line_to(cr,	last_x_px,	max_y);
						/* then draw a line connecting last point to this point */
						cairo_move_to(cr,	last_x_px,	last_y_px);
						cairo_line_to(cr,	x_px,	y_px);
						min_y = max_y = y_px;
					}
					else {
						if(y_px > max_y) 
							max_y = y_px;
						if(y_px < min_y) 
							min_y = y_px;
					}
				}
				last_x_px = x_px;
				last_y_px = y_px;
			}
		}
		else {
			for(j = 0; j < t->length; j += dd) {
				int n = t->start_index + j;
				if(n >= t->capacity) {
					n -= t->capacity;
				}
				if(isnan(t->y_data[n])) {
					last_was_NAN = 1;
					continue;
				}
				char this_is_out = 0;
				if(t->x_data[n] < x_axis->min_val ||
					 t->x_data[n] > x_axis->max_val ||
					 t->y_data[n] < y_axis->min_val || 
					 t->y_data[n] > y_axis->max_val
				) {
					this_is_out = 1;
				}
				double x_px = x_m * t->x_data[n] + x_b;
				double y_px = y_m * t->y_data[n] + y_b;
				if(first_pt) {
					cairo_move_to(cr,	x_px,	y_px);
					first_pt = 0;
				}
				else if(!this_is_out && last_was_NAN) {
					cairo_move_to(cr,	x_px,	y_px);
				}
				else if(!this_is_out && last_was_out) {
					cairo_line_to(cr,	x_px,	y_px);
				}
				else if(this_is_out && !last_was_out) {
					cairo_line_to(cr,	x_px,	y_px);
				}
				else if(this_is_out && last_was_out) {
					cairo_move_to(cr,	x_px,	y_px);
				}
				else {
					cairo_line_to(cr,	x_px,	y_px);
				}
				last_was_NAN = 0;
				last_was_out = this_is_out;
			}
		}
		cairo_stroke(cr);
	}
	cairo_restore(cr);

	// now draw the trace markers (if requested)
	for(i = 0; i < p->num_traces; i++) {
		cairo_save(cr);
		trace_t *t = p->traces[i];
		if(t->marker_type == MARKER_NONE) {
			continue;
		}
		cairo_set_source_rgb (cr, t->marker_color.red, t->marker_color.green, t->marker_color.blue);
		if(t->marker_type == MARKER_POINT) {
			cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
		}
		if(t->length <= 0) continue;
		int dd = t->decimate_divisor;
		for(j = 0; j < t->length; j += dd) {
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
		cairo_restore(cr);
	}


/*
	// DEBUG!!!!!
	// Draw the legend image
	cairo_save(cr);
	cairo_set_source_surface(cr, legend_buffer, 50, 50);
	cairo_rectangle(cr, 50, 50, 30, 100);
	cairo_clip(cr);
	cairo_paint(cr);
	cairo_restore(cr);
*/
		
	return FALSE;
}

/* This get's called by GtkMain when the widget is resized. 
 * We'll use it to resize (reallocate) our plot image buffer
 */
static gboolean jbplot_configure (GtkWidget *plot, GdkEventConfigure *event) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);

	double width = plot->allocation.width;
	double height = plot->allocation.height;

	if(priv->plot_context != NULL) {
		cairo_destroy(priv->plot_context);
	}
	if(priv->plot_buffer != NULL) {
		cairo_surface_destroy(priv->plot_buffer);
	}
	priv->plot_buffer = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	cairo_status_t stat = cairo_surface_status(priv->plot_buffer);
	if(stat != CAIRO_STATUS_SUCCESS) {
		printf("Error creating cairo image surface: %s\n", cairo_status_to_string(stat));
	}
	priv->plot_context = cairo_create(priv->plot_buffer);
	cairo_status_t cr_stat = cairo_status(priv->plot_context);
	if(cr_stat != CAIRO_STATUS_SUCCESS) {
		printf("Error creating cairo image context: %s\n", cairo_status_to_string(cr_stat));
		return FALSE;
	}
	priv->needs_redraw = TRUE;
	priv->plot.legend.needs_redraw = 1;
	return FALSE;
}

static gboolean jbplot_expose (GtkWidget *plot, GdkEventExpose *event) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);

	plot_t *p = &(priv->plot);
	axis_t *x_axis = &(p->x_axis);
	axis_t *y_axis = &(p->y_axis);
	plot_area_t *pa = &(p->plot_area);
	legend_t *l = &(p->legend);
	
	double x_m = priv->x_m;	
	double y_m = priv->y_m;	
	double x_b = priv->x_b;	
	double y_b = priv->y_b;	

	cairo_t *cr = gdk_cairo_create(plot->window);
	if(priv->antialias) {
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_DEFAULT);
	}
	else {
		cairo_set_antialias(cr, CAIRO_ANTIALIAS_NONE);
	}

	// make sure the plot_context is valid (no latched errors)
	// If invalid, create a new one...
	cairo_status_t cr_stat = cairo_status(priv->plot_context);
	if(cr_stat != CAIRO_STATUS_SUCCESS) {
		//printf("Invalid cairo context: %s\n", cairo_status_to_string(cr_stat));
		cairo_destroy(priv->plot_context);
		priv->plot_context = cairo_create(priv->plot_buffer);
	}

	/* Draw the plot to the plot image buffer */
	draw_plot(plot, priv->plot_context, plot->allocation.width, plot->allocation.height);

	/* Then paint the plot image buffer on the widget itself */
	cairo_save(cr);
	cairo_set_source_surface(cr, priv->plot_buffer, 0, 0);
	cairo_rectangle(cr, 0, 0, plot->allocation.width, plot->allocation.height);
	cairo_clip(cr);
	cairo_paint(cr);
	cairo_restore(cr);
	
	/********************** draw the cursor (if needed) *************************/
	if(p->cursor.type != CURSOR_NONE) {
		cursor_t *c = &(p->cursor);
		cairo_save(cr);
		cairo_set_source_rgb (cr, c->color.red, c->color.green, c->color.blue);
		cairo_set_line_width(cr, c->line_width);
		if(c->line_type == LINETYPE_SOLID) {
			cairo_set_dash(cr, dash_pattern, 0, 0);
		}	
		else if(c->line_type == LINETYPE_DASHED) {
			cairo_set_dash(cr, dash_pattern, 2, 0);
		}	
		else if(c->line_type == LINETYPE_DOTTED) {
			cairo_set_dash(cr, dot_pattern, 2, 0);
		}
		double x_px = x_m * c->x + x_b;	
		double y_px = y_m * c->y + y_b;	
		if(c->type == CURSOR_VERT || c->type == CURSOR_CROSS) {
			if(x_px >= p->plot_area.left_edge && x_px <= p->plot_area.right_edge) {
				cairo_move_to(cr, x_px, p->plot_area.top_edge);
				cairo_line_to(cr, x_px, p->plot_area.bottom_edge);
				cairo_stroke(cr);
			}
		}
		if(c->type == CURSOR_HORIZ || c->type == CURSOR_CROSS) {
			if(y_px >= p->plot_area.top_edge && y_px <= p->plot_area.bottom_edge) {
				cairo_move_to(cr, p->plot_area.left_edge, y_px);
				cairo_line_to(cr, p->plot_area.right_edge, y_px);
				cairo_stroke(cr);
			}
		}
		cairo_restore(cr);
	}


	/************* draw the zoom box if zooming is active *****************/
	if(priv->zooming) {
		double dashes[] = {8.0,4.0};
		cairo_save(cr);
		cairo_set_source_rgba (cr, 0.423, 0.646, 0.784, 0.3);
		cairo_set_line_width (cr, 1.0);
		double x_e = priv->drag_end_x;
		double y_e = priv->drag_end_y;
		double x_s = priv->drag_start_x;
		double y_s = priv->drag_start_y;
		if(priv->drag_end_x < priv->plot.plot_area.left_edge) {
			x_e = priv->plot.plot_area.left_edge;
		}
		else if(priv->drag_end_x > priv->plot.plot_area.right_edge) {
			x_e = priv->plot.plot_area.right_edge;
		}
		if(priv->drag_end_y < priv->plot.plot_area.top_edge) {
			y_e = priv->plot.plot_area.top_edge;
		}
		else if(priv->drag_end_y > priv->plot.plot_area.bottom_edge) {
			y_e = priv->plot.plot_area.bottom_edge;
		}
		if(priv->h_zoom) {
			y_s = priv->plot.plot_area.top_edge;
			y_e = priv->plot.plot_area.bottom_edge;
		}
		if(priv->v_zoom) {
			x_s = priv->plot.plot_area.left_edge;
			x_e = priv->plot.plot_area.right_edge;
		}
			
		cairo_rectangle(cr, x_s, y_s,	x_e - x_s, y_e - y_s);
		cairo_fill_preserve(cr);
		cairo_set_source_rgb (cr, 0.423, 0.646, 0.784);
		cairo_stroke(cr);	
		cairo_restore(cr);
	}

	/********** find closest data point if showing coords or cross-hair *****/
	double x_px = 0.0;
	double y_px = 0.0;
	int closest_point_index = 0;
	int closest_trace_index = 0;
	gboolean is_in_plot_area = FALSE;
	if(priv->do_snap_to_data && (priv->do_show_cross_hair || priv->do_show_coords)) {
		gint x,y;
		gtk_widget_get_pointer(plot, &x, &y);
		if(x >= priv->plot.plot_area.left_edge-1 && x <= priv->plot.plot_area.right_edge+1 &&
		   y >= priv->plot.plot_area.top_edge-1 &&  y <= priv->plot.plot_area.bottom_edge+1) {
			int i, j;
			double min_dist = DBL_MAX;
			is_in_plot_area = TRUE;
			for(j=0; j < p->num_traces; j++) {
				for(i=0; i<(p->traces[0])->length; i++) {
					double dist;
					dist = pow((p->traces[j])->x_data[i] * x_m + x_b - x,2) + pow((p->traces[j])->y_data[i] * y_m + y_b - y,2);
					if(dist < min_dist) {
						min_dist = dist;
						closest_point_index = i;
						closest_trace_index = j;
					}
				}
			}
			x_px = x_m * (p->traces[closest_trace_index])->x_data[closest_point_index] + x_b;
			y_px = y_m * (p->traces[closest_trace_index])->y_data[closest_point_index] + y_b;
		}
		else {
			is_in_plot_area = FALSE;
		}
	}

	/************** draw crosshair if active ****************************/
	if(priv->do_show_cross_hair) {
		gint x,y;
		int inhibit_ch = 0;
		gtk_widget_get_pointer(plot, &x, &y);
		if(x < priv->plot.plot_area.left_edge-1) {
			x = priv->plot.plot_area.left_edge;
			inhibit_ch = 1;
		}
		if(x > priv->plot.plot_area.right_edge+1) {
			x = priv->plot.plot_area.right_edge;
			inhibit_ch = 1;
		}
		if(y < priv->plot.plot_area.top_edge-1) {
			y = priv->plot.plot_area.top_edge;
			inhibit_ch = 1;
		}
		if(y > priv->plot.plot_area.bottom_edge+1) {
			y = priv->plot.plot_area.bottom_edge;
			inhibit_ch = 1;
		}
		if(priv->do_snap_to_data && is_in_plot_area) {
			x = x_px;
			y = y_px;
		}
		if(!inhibit_ch) {
			cairo_save(cr);
			//cairo_set_source_rgb (cr, 1.0, 1.0, 0.0); // yellow
			cairo_set_source_rgb (cr, 0.0, 0.0, 1.0); // blue
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
			priv->cross_hair_is_visible = TRUE;
		}
		else {
			priv->cross_hair_is_visible = FALSE;
		}
	}

	/**************** draw coordinates if active (whether zooming or not) *************/
	if(priv->do_show_coords) {
		gint x,y;
		gtk_widget_get_pointer(plot, &x, &y);
		if(priv->do_snap_to_data && is_in_plot_area) {
			x = x_px;
			y = y_px;
		}
		if(x >= (priv->plot.plot_area.left_edge-1) &&
		   x <= (priv->plot.plot_area.right_edge+1) &&
		   y >= (priv->plot.plot_area.top_edge-1) &&
		   y <= (priv->plot.plot_area.bottom_edge+1)
		  ) {
			char x_str[100], y_str[100];
			double x_w, x_h, y_w, y_h;
			double box_w, box_h;

			char x_fs[200]="x=";
			char y_fs[200]="y=";
			strcat(x_fs, p->x_axis.coord_label_format_string);
			strcat(y_fs, p->y_axis.coord_label_format_string);

			if(priv->do_snap_to_data) {
				sprintf(x_str, x_fs, (p->traces[closest_trace_index])->x_data[closest_point_index]);
				sprintf(y_str, y_fs, (p->traces[closest_trace_index])->y_data[closest_point_index]);
			}
			else {
				sprintf(x_str, x_fs, ((double)x-x_b)/x_m);
				sprintf(y_str, y_fs, ((double)y-y_b)/y_m);
			}
			x_w = get_text_width(cr, x_str, 10);
			y_w = get_text_width(cr, y_str, 10);
			x_h = get_text_height(cr, x_str, 10);
			y_h = get_text_height(cr, y_str, 10);
			box_w = 10 + ((x_w > y_w) ? x_w : y_w);
			box_h = 10 + (x_h + y_h);

			gint mid_x = (priv->plot.plot_area.left_edge + priv->plot.plot_area.right_edge)/2.;
			gint mid_y = (priv->plot.plot_area.top_edge + priv->plot.plot_area.bottom_edge)/2.;

			cairo_save(cr);

			// draw white rectangle
			cairo_set_source_rgb (cr, 1, 1, 1);
			if(x < mid_x) { // left half
				if(y > mid_y) { // bottom left quadrant
					cairo_rectangle(cr, x+3, y-box_h-3, box_w, box_h);
				}
				else { // top left quadrant
					cairo_rectangle(cr, x+3, y+3, box_w, box_h);
				}
			}
			else { // right half
				if(y > mid_y) { // bottom right quadrant
					cairo_rectangle(cr, x-box_w-3, y-box_h-3, box_w, box_h);
				}
				else { // top right quadrant
					cairo_rectangle(cr, x-box_w-3, y+3, box_w, box_h);
				}
			}
			cairo_fill_preserve(cr);
			cairo_set_source_rgb (cr, 0, 0, 0);
			cairo_set_line_width (cr, 1.0);
			cairo_stroke(cr);

			// draw coordinates...
			cairo_set_source_rgb (cr, 0, 0, 0);
			if(x < mid_x) { // left half
				if(y > mid_y) { // bottom left quadrant
					draw_horiz_text_at_point(cr, x_str, x+5+3, y-5-y_h-3-2, ANCHOR_BOTTOM_LEFT);
					draw_horiz_text_at_point(cr, y_str, x+5+3, y-5-3+2, ANCHOR_BOTTOM_LEFT);
				}
				else { // top left quadrant
					draw_horiz_text_at_point(cr, x_str, x+5+3, y+6, ANCHOR_TOP_LEFT);
					draw_horiz_text_at_point(cr, y_str, x+5+3, y+5+y_h+3+2, ANCHOR_TOP_LEFT);
				}
			}
			else { // right half
				if(y > mid_y) { // bottom right quadrant
					draw_horiz_text_at_point(cr, x_str, x-box_w+5-3, y-5-y_h-3-2, ANCHOR_BOTTOM_LEFT);
					draw_horiz_text_at_point(cr, y_str, x-box_w+5-3, y-5-3+2, ANCHOR_BOTTOM_LEFT);
				}
				else { // top right quadrant
					draw_horiz_text_at_point(cr, x_str, x-box_w+5-3, y+6, ANCHOR_TOP_LEFT);
					draw_horiz_text_at_point(cr, y_str, x-box_w+5-3, y+5+y_h+3+2, ANCHOR_TOP_LEFT);
				}
			}

			cairo_restore(cr);
		}
			
	}

	cairo_destroy(cr);
	return FALSE;
}



/******************* Plot Drawing Functions **************************/
static int set_major_tic_values(axis_t *a, double min, double max) {
	double raw_range = max - min;
	double raw_tic_delta = raw_range / (a->num_request_major_tics - 1);
	double mantissa;
	int exponent;
	get_double_parts(raw_tic_delta, &mantissa, &exponent);
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
		/* perform check to see if it should be equal to zero */
		if(fabs(tic_val / actual_tic_delta) < 0.5) {
			tic_val = 0.0;
		}
		if(!a->do_manual_tics) {
			a->major_tic_values[i] = tic_val;
		}
		i++;
	}
	if(i >= MAX_NUM_MAJOR_TICS) {
		printf("Too many major tics!!!\n");
		return -1;
	}
	
	if(a->do_autoscale && a->do_loose_fit) {
		a->max_val = tic_val;	
		if(!a->do_manual_tics) {
			a->major_tic_values[i] = tic_val;
			a->num_actual_major_tics = i+1;
		}
	}
	else {
		a->max_val = max;		
		if(!a->do_manual_tics) {
			a->num_actual_major_tics = i;		
		}
	}
	a->major_tic_delta = actual_tic_delta;
	return 0;
}


static int set_major_tic_labels(axis_t *a) {
	int i, ret;
	int err = 0;
	if(a->do_auto_tic_format) { // do automagic formatting...
		double dd = log10( fabs(a->major_tic_delta) );
		double d1 = log10(fabs(a->major_tic_values[0]));
		double d2 = log10(fabs(a->major_tic_values[a->num_actual_major_tics-1]));
		double d = (d2>d1) ? d2 : d1;
		int sigs = ceil(d) - floor(dd) + 1.5;
		if(sigs < 4) 
			sigs = 4;
		sprintf(a->tic_label_format_string, "%%.%dg", sigs);
		sprintf(a->coord_label_format_string, "%%.%dg", sigs+2);
		for(i=0; i<a->num_actual_major_tics; i++) { 
			ret = snprintf(a->major_tic_labels[i], MAJOR_TIC_LABEL_SIZE, a->tic_label_format_string, a->major_tic_values[i]);
		}
	}
	else { // otherwise, use standard fprintf type of format string
		for(i=0; i<a->num_actual_major_tics; i++) { 
			ret = snprintf(a->major_tic_labels[i], MAJOR_TIC_LABEL_SIZE, a->tic_label_format_string, a->major_tic_values[i]);
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
      

static void get_double_parts(double f, double *mantissa, int *exponent) {
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
  double min = DBL_MAX, max = -DBL_MAX;
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
	if(min == max) {
		if(min == 0) {
			min = -1.0;
			max = +1.0;
		}
		else {
			min = min - 0.1 * fabs(min);
			max = max + 0.1 * fabs(max);
		}
	}
  r.min = min;
  r.max = max;
  return r;
}

static data_range get_y_range_within_x_range(trace_t **traces, int num_traces, data_range xr) {
  data_range r;
  int i, j;
  double min = DBL_MAX, max = -DBL_MAX;
  for(i = 0; i < num_traces; i++) {
    trace_t *t = traces[i];
    for(j = 0; j< t->length; j++) {
			if(t->x_data[j] >= xr.min && t->x_data[j] <= xr.max) {
				if(t->y_data[j] > max) {
					max = t->y_data[j];
				}
				if(t->y_data[j] < min) {
					min = t->y_data[j];
				}
			}
    }
  }
	if(min == max) {
		if(min == 0) {
			min = -1.0;
			max = +1.0;
		}
		else {
			min = min - 0.1 * min;
			max = max + 0.1 * max;
		}
	}
  r.min = min;
  r.max = max;
  return r;
}

static data_range get_x_range(trace_t **traces, int num_traces) {
  data_range r;
  int i, j;
  double min = DBL_MAX, max = -DBL_MAX;
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
	if(min == max) {
		if(min == 0) {
			min = -1.0;
			max = +1.0;
		}
		else {
			min = min - 0.1 * fabs(min);
			max = max + 0.1 * fabs(max);
		}
	}
  r.min = min;
  r.max = max;
  return r;
}

static data_range get_x_range_within_y_range(trace_t **traces, int num_traces, data_range yr) {
  data_range r;
  int i, j;
  double min = DBL_MAX, max = -DBL_MAX;
  for(i = 0; i < num_traces; i++) {
    trace_t *t = traces[i];
    for(j = 0; j< t->length; j++) {
			if(t->y_data[j] >= yr.min && t->y_data[j] <= yr.max) {
				if(t->x_data[j] > max) {
					max = t->x_data[j];
				}
				if(t->x_data[j] < min) {
					min = t->x_data[j];
				}
			}
    }
  }
	if(min == max) {
		if(min == 0) {
			min = -1.0;
			max = +1.0;
		}
		else {
			min = min - 0.1 * min;
			max = max + 0.1 * max;
		}
	}
  r.min = min;
  r.max = max;
  return r;
}

/******************** Public Functions *******************************/
int jbplot_set_x_axis_tics(jbplot *plot, int n, double *values, char **labels) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE((plot));
	int ret = 0;
	if(n < 2) { // must specify at least 2 values, otherwise use auto mode
		priv->plot.x_axis.do_manual_tics = 0;
	}
	else if(n > MAX_NUM_MAJOR_TICS) { // if too many, just reject it...
		priv->plot.x_axis.do_manual_tics = 0;
		ret = -1;
	}
	else {
		priv->plot.x_axis.num_actual_major_tics = n;		
		priv->plot.x_axis.do_manual_tics = 1;
		int i;
		for(i=0; i<n; i++) {
			priv->plot.x_axis.major_tic_values[i] = values[i];
			strncpy(priv->plot.x_axis.major_tic_labels[i], labels[i], MAJOR_TIC_LABEL_SIZE-1);
		}
	}
	priv->needs_redraw = TRUE;
	gtk_widget_queue_draw((GtkWidget *)plot);
	return ret;
}

int jbplot_set_y_axis_tics(jbplot *plot, int n, double *values, char **labels) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE((plot));
	int ret = 0;
	if(n < 2) { // must specify at least 2 values, otherwise use auto mode
		priv->plot.y_axis.do_manual_tics = 0;
	}
	else if(n > MAX_NUM_MAJOR_TICS) { // if too many, just reject it...
		priv->plot.y_axis.do_manual_tics = 0;
		ret = -1;
	}
	else {
		priv->plot.y_axis.num_actual_major_tics = n;		
		priv->plot.y_axis.do_manual_tics = 1;
		int i;
		for(i=0; i<n; i++) {
			priv->plot.y_axis.major_tic_values[i] = values[i];
			strncpy(priv->plot.y_axis.major_tic_labels[i], labels[i], MAJOR_TIC_LABEL_SIZE-1);
		}
	}
	priv->needs_redraw = TRUE;
	gtk_widget_queue_draw((GtkWidget *)plot);
	return ret;
}



int jbplot_set_coords_visible(jbplot *plot, int vis) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE((plot));
	priv->do_show_coords = vis;
	priv->needs_redraw = TRUE;
	gtk_widget_queue_draw((GtkWidget *)plot);
	return 0;
}	

int jbplot_get_crosshair_mode(jbplot *plot) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE((plot));
	if(priv->do_show_cross_hair) {
		if(priv->do_snap_to_data) {
			return CROSSHAIR_SNAP;
		}
		else {
			return CROSSHAIR_FREE;
		}
	}
	else {
		return CROSSHAIR_NONE;
	}
}

int jbplot_set_crosshair_mode(jbplot *plot, crosshair_t mode) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE((plot));
	switch(mode) {
		case CROSSHAIR_NONE:
			priv->do_show_cross_hair = 0;
			break;
		case CROSSHAIR_FREE:
			priv->do_show_cross_hair = 1;
			priv->do_snap_to_data = 0;
			break;
		case CROSSHAIR_SNAP:
			priv->do_show_cross_hair = 1;
			priv->do_snap_to_data = 1;
			break;
		default:
			break;
	}
	priv->needs_redraw = TRUE;
	gtk_widget_queue_draw((GtkWidget *)plot);
	return 0;
}

int jbplot_set_x_axis_format(jbplot *plot, char *str) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	if(strlen(str)==0) { // empty string -> do auto formatting
		priv->plot.x_axis.do_auto_tic_format = 1;
	}
	else {
		priv->plot.x_axis.do_auto_tic_format = 0;
  	strcpy(priv->plot.x_axis.tic_label_format_string, str);
	}
	return 0;
}

int jbplot_set_y_axis_format(jbplot *plot, char *str) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	if(strlen(str)==0) { // empty string -> do auto formatting
		priv->plot.y_axis.do_auto_tic_format = 1;
	}
	else {
		priv->plot.y_axis.do_auto_tic_format = 0;
		strcpy(priv->plot.y_axis.tic_label_format_string, str);
	}
	return 0;
}

int jbplot_set_cursor_pos(jbplot *plot, double x, double y) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	priv->plot.cursor.x = x;
	priv->plot.cursor.y = y;
	gtk_widget_queue_draw((GtkWidget *)plot);
	return 0;
}

int jbplot_set_cursor_props(
	jbplot *plot, 
	cursor_type_t type, 
	rgb_color_t color,
	double line_width, 
	int line_type)
{
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	priv->plot.cursor.type = type;
	priv->plot.cursor.line_width = line_width;	
	priv->plot.cursor.line_type = line_type;	
	priv->plot.cursor.color = color;
	return 0;	
}

int jbplot_set_antialias(jbplot *plot, gboolean state) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	if(state) {
		priv->antialias = 1;
	}
	else {
		priv->antialias = 0;
	}
	priv->needs_redraw = TRUE;
	return 0;
}


GtkWidget *jbplot_new (void) {
	return g_object_new (JBPLOT_TYPE, NULL);
}


void jbplot_destroy(jbplot *plot) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	
	if(priv->legend_context != NULL) {
		cairo_destroy(priv->legend_context);
	}
	if(priv->legend_buffer != NULL) {
		cairo_surface_destroy(priv->legend_buffer);
	}

	if(priv->plot_context != NULL) {
		cairo_destroy(priv->plot_context);
	}
	if(priv->plot_buffer != NULL) {
		cairo_surface_destroy(priv->plot_buffer);
	}
}


int jbplot_capture_svg(jbplot *plot, char *filename) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	cairo_surface_t *svg_surf;
	svg_surf = (cairo_surface_t *)cairo_svg_surface_create((const char *)filename, 600., 400.);

	cairo_t *cr = cairo_create(svg_surf);
	priv->needs_redraw = TRUE;
	priv->plot.legend.needs_redraw = 1;
	draw_plot((GtkWidget *)plot, cr, 600, 400);
	cairo_show_page(cr);
	cairo_destroy(cr);

	cairo_surface_flush(svg_surf);
	cairo_surface_destroy(svg_surf);
	return 0;
}


int jbplot_capture_png(jbplot *plot, char *filename, int width, int height) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	priv->needs_redraw = TRUE;
	priv->plot.legend.needs_redraw = 1;
	if(width<1 || height<1) { // draw at present size
		draw_plot( 
			(GtkWidget *)plot, 
			priv->plot_context, 
			((GtkWidget *)plot)->allocation.width, 
			((GtkWidget *)plot)->allocation.height
		);
		cairo_surface_write_to_png(priv->plot_buffer, filename);
	}
	else { // user-specified size 
		cairo_surface_t *png_surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
		cairo_t *cr = cairo_create(png_surf);
		draw_plot((GtkWidget *)plot, cr, width, height);
		cairo_surface_write_to_png(png_surf, filename);
		cairo_destroy(priv->plot_context);
		cairo_surface_destroy(priv->plot_buffer);
	}
	return 0;
}

int jbplot_trace_set_decimation(trace_handle th, int divisor) {
	/* divisor value less than 1 means lossless decimation */
	if(divisor < 1) {
		th->decimate_divisor = 1;
		th->lossless_decimation = 1;
	}
	else {
		th->decimate_divisor = divisor;
		th->lossless_decimation = 0;
	}
	return 0;
}



int jbplot_trace_set_data(trace_handle th, double *x_start, double *y_start, int length) {
	if(th->is_data_owner) {
		free(th->x_data);
		free(th->y_data);
		th->is_data_owner = 0;
	}
	th->x_data = x_start;
	th->y_data = y_start;
	th->length = length;
	th->capacity = length;
	th->start_index = 0;
	th->end_index = length-1;
	return 0;
}

int jbplot_trace_resize(trace_handle th, int new_size) {
	if(!th->is_data_owner) {
		th->capacity = new_size;
	}
	else {
		if(new_size >= th->capacity) {
			double *px, *py;
			px = realloc(th->x_data, new_size * sizeof(double));
			py = realloc(th->y_data, new_size * sizeof(double));
			if(px==NULL || py==NULL) {
				return -1;
			} 
			th->x_data = px;
			th->y_data = py;
			th->capacity = new_size;
		}
	}	
	return 0;
}

int jbplot_trace_set_name(trace_t *t, char *name) {
	t->name[0] = '\0';
	strncat(t->name, name, MAX_TRACE_NAME_LENGTH);
	return 0;
}

int jbplot_clear_data(jbplot *p) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(p);
	int i;
	for(i=0; i<priv->plot.num_traces; i++) {
		jbplot_trace_clear_data(priv->plot.traces[i]);
	}
	priv->needs_redraw = TRUE;
	gtk_widget_queue_draw((GtkWidget *)p);
	return 0;
}

int jbplot_trace_clear_data(trace_t *t) {
	t->length = 0;
	t->start_index = 0;
	t->end_index = 0;
	return 0;
}


int jbplot_legend_refresh(jbplot *plot) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	priv->plot.legend.needs_redraw = 1;
	priv->needs_redraw = TRUE;
	gtk_widget_queue_draw((GtkWidget *)plot);
	return 0;
}
	

int jbplot_set_legend_props(jbplot *plot, double border_width, rgb_color_t *bg_color, rgb_color_t *border_color, legend_pos_t position) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	priv->plot.legend.position = position;
	if(position == LEGEND_POS_NONE) {
		return 0;
	}
	if(border_width > 0) {
		priv->plot.legend.bounding_box_width = border_width;
		priv->plot.legend.do_show_bounding_box = 1;
	}
	else {
		priv->plot.legend.do_show_bounding_box = 0;
	}
	if(bg_color != NULL) {
		priv->plot.legend.bg_color = *bg_color;
	}
	if(border_color != NULL) {
		priv->plot.legend.border_color = *border_color;
	}
	priv->needs_redraw = TRUE;
	priv->plot.legend.needs_redraw = 1;
	return 0;	
}

int jbplot_set_x_axis_gridline_visible(jbplot *plot, gboolean visible) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	if(visible) {
		priv->plot.x_axis.do_show_major_gridlines = 1;	
	}
	else {
		priv->plot.x_axis.do_show_major_gridlines = 0;
	}
	priv->needs_redraw = TRUE;
	gtk_widget_queue_draw((GtkWidget *)plot);
	return 0;
}

int jbplot_set_y_axis_gridline_visible(jbplot *plot, gboolean visible) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	if(visible) {
		priv->plot.y_axis.do_show_major_gridlines = 1;	
	}
	else {
		priv->plot.y_axis.do_show_major_gridlines = 0;
	}
	priv->needs_redraw = TRUE;
	gtk_widget_queue_draw((GtkWidget *)plot);
	return 0;
}

int jbplot_set_x_axis_gridline_props(jbplot *plot, line_type_t type, double width, rgb_color_t *color) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	priv->plot.x_axis.major_gridline_width = width;	
	if(color != NULL) {
		priv->plot.x_axis.major_gridline_color = *color;	
	}
	priv->plot.x_axis.major_gridline_type = type;
	priv->needs_redraw = TRUE;
	gtk_widget_queue_draw((GtkWidget *)plot);
	return 0;
}

int jbplot_set_y_axis_gridline_props(jbplot *plot, line_type_t type, double width, rgb_color_t *color) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	priv->plot.y_axis.major_gridline_width = width;	
	if(color != NULL) {
		priv->plot.y_axis.major_gridline_color = *color;	
	}
	priv->plot.y_axis.major_gridline_type = type;
	priv->needs_redraw = TRUE;
	gtk_widget_queue_draw((GtkWidget *)plot);
	return 0;
}

int jbplot_set_bg_color(jbplot *plot, rgb_color_t *color) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	if(color != NULL) {
		priv->plot.bg_color = *color;
	}
	priv->needs_redraw = TRUE;
	gtk_widget_queue_draw((GtkWidget *)plot);
	return 0;
}

int jbplot_set_plot_area_color(jbplot *plot, rgb_color_t *color) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	if(color != NULL) {
		priv->plot.plot_area.bg_color = *color;
	}
	priv->needs_redraw = TRUE;
	gtk_widget_queue_draw((GtkWidget *)plot);
	return 0;
}

int jbplot_set_plot_area_border(jbplot *plot, double width, rgb_color_t *color) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	priv->plot.plot_area.bounding_box_width = width;
	if(color != NULL) {
		priv->plot.plot_area.border_color = *color;
	}
	if(width > 0) {
		priv->plot.plot_area.do_show_bounding_box = 1;
	}
	else {
		priv->plot.plot_area.do_show_bounding_box = 0;
	}
	priv->needs_redraw = TRUE;
	gtk_widget_queue_draw((GtkWidget *)plot);
	return 0;
}

int jbplot_get_ideal_LR_margins(jbplot *plot, double *left, double *right) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	if (
		priv->plot_context == NULL || 
		cairo_status(priv->plot_context) != CAIRO_STATUS_SUCCESS || 
		((GtkWidget *)plot)->allocation.width <= 0
	) {
		return -1;
	}
	priv->get_ideal_lr = TRUE;
	priv->needs_redraw = TRUE;
	draw_plot((GtkWidget *)plot, priv->plot_context, ((GtkWidget *)plot)->allocation.width, ((GtkWidget *)plot)->allocation.height);
	priv->get_ideal_lr = FALSE;
	*left = priv->plot.plot_area.ideal_left_margin;
	*right = priv->plot.plot_area.ideal_right_margin;
	return 0;
}

int jbplot_set_plot_area_LR_margins(jbplot *plot, margin_mode_t mode, double left, double right) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	switch(mode) {
		case MARGIN_AUTO:
			priv->plot.plot_area.LR_margin_mode = MARGIN_AUTO;
			break;
		case MARGIN_PERCENT:
		case MARGIN_PX:
			priv->plot.plot_area.LR_margin_mode = mode;
			priv->plot.plot_area.lmargin = left;
			priv->plot.plot_area.rmargin = right;
			break;
		default:
			priv->plot.plot_area.LR_margin_mode = MARGIN_AUTO;
	}
	priv->needs_redraw = TRUE;
	gtk_widget_queue_draw((GtkWidget *)plot);
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
	priv->needs_redraw = TRUE;
	gtk_widget_queue_draw((GtkWidget *)plot);
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
	priv->needs_redraw = TRUE;
	gtk_widget_queue_draw((GtkWidget *)plot);
	return 0;
}

int jbplot_get_x_axis_range(jbplot *plot, double *min, double *max) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	*min = priv->plot.x_axis.min_val;
	*max = priv->plot.x_axis.max_val;
	return 0;
}

int jbplot_set_x_axis_range(jbplot *plot, double min, double max) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	priv->plot.x_axis.do_autoscale = 0;
	priv->plot.x_axis.min_val = min;
	priv->plot.x_axis.max_val = max;
	//printf("Setting x-axis range to (%g , %g)\n", min, max);
	priv->needs_redraw = TRUE;
	gtk_widget_queue_draw((GtkWidget *)plot);
	return 0;
}
	
int jbplot_get_y_axis_range(jbplot *plot, double *min, double *max) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	*min = priv->plot.y_axis.min_val;
	*max = priv->plot.y_axis.max_val;
	return 0;
}


int jbplot_set_y_axis_range(jbplot *plot, double min, double max) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	priv->plot.y_axis.do_autoscale = 0;
	priv->plot.y_axis.min_val = min;
	priv->plot.y_axis.max_val = max;
	//printf("Setting y-axis range to (%g , %g)\n", min, max);
	priv->needs_redraw = TRUE;
	gtk_widget_queue_draw((GtkWidget *)plot);
	return 0;
}
	
void jbplot_refresh(jbplot *plot) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	priv->needs_redraw = TRUE;
	gtk_widget_queue_draw((GtkWidget *)plot);
	return;
}

int jbplot_trace_set_line_props(trace_t *t, line_type_t type, double width, rgb_color_t *color) {
	t->line_type = type;
	t->line_width = width;
	if(color != NULL) {
		t->line_color = *color;
	}
	return 0;
}

int jbplot_trace_set_marker_props(trace_t *t, marker_type_t type, double size, rgb_color_t *color) {
	t->marker_type = type;
	t->marker_size = size;
	if(color != NULL) {
		t->marker_color = *color;
	}
	return 0;
}

trace_t *jbplot_create_trace_with_external_data(double *x, double *y, int length, int capacity) {
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
	t->decimate_divisor = 1;
	t->lossless_decimation = 0;
	strcpy(t->name, "trace");
	return t;
}

int jbplot_trace_add_point(trace_t *t, double x, double y) {
	if(!t->is_data_owner) {
		return -1;
	}
	if(t->length >= t->capacity) {
		t->x_data[t->start_index] = x;
		t->y_data[t->start_index] = y;
		t->start_index++;
		if(t->start_index >= t->capacity) {
			t->start_index = 0;
		}
	}
	else {
		int index;
		index = t->start_index + t->length;
		if(index >= t->capacity) {
			index = 0;
		}
		t->x_data[index] = x;
		t->y_data[index] = y;
		t->length++;
	}
	t->end_index = t->start_index + t->length - 1;
	if(t->end_index >= t->capacity) {
		t->end_index = 0;
	}
	return 0;
}


trace_t *jbplot_create_trace(int capacity) {
	trace_t *t;
	t = malloc(sizeof(trace_t));
	if(t==NULL) {
		return NULL;
	}	
	if(capacity > 0) {
		t->x_data = malloc(sizeof(double)*capacity);
		if(t->x_data==NULL) {
			free(t);
			return NULL;
		}
		t->y_data = malloc(sizeof(double)*capacity);
		if(t->y_data==NULL) {
			free(t);
			free(t->x_data);
			return NULL;
		}
		t->is_data_owner = 1;
	}
	else {
		t->is_data_owner = 0;
	}
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
	t->decimate_divisor = 1;
	t->lossless_decimation = 0;
	strcpy(t->name, "trace_name");

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

int jbplot_remove_trace(jbplot *plot, trace_handle th) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	plot_t *p = &(priv->plot);
	int i;
	int trace_index = -1;

	/* can't remove trace is there are none */
	if(p->num_traces < 1) {
		return -1;
	}

	/* find index of trace to remove */
	for(i = 0; i < p->num_traces; i++) {
		if(p->traces[i] == th) {
			trace_index = i;
			break;
		}
	}

	/* not found */
	if(trace_index < 0) {
		return -1;
	}

	/* if it's the last trace, this is easy */
	if(trace_index == p->num_traces - 1) {
		p->num_traces--;
		jbplot_refresh(plot);
		return 0;
	}
	
	/* if it's in the middle, slide eveything down one slot */
	memmove(
		p->traces + trace_index, 
		p->traces + trace_index + 1, 
		(p->num_traces - trace_index - 1)*sizeof(trace_t *)
	);
	p->num_traces--;
	jbplot_refresh(plot);
	return 0;
}

int jbplot_add_trace(jbplot *plot, trace_t *t) {
	jbplotPrivate *priv = JBPLOT_GET_PRIVATE(plot);
	plot_t *p = &(priv->plot);

	if(p->num_traces + 1 >= MAX_NUM_TRACES) {
		return -1;
	}
	p->traces[p->num_traces] = t;
	(p->num_traces)++;
	jbplot_refresh(plot);
	return (p->num_traces)-1;
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

	jbplot_refresh(plot);
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
	jbplot_refresh(plot);
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
	jbplot_refresh(plot);
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
	jbplot_refresh(plot);
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
	jbplot_refresh(plot);
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
	jbplot_refresh(plot);
	return 0;
}
	
