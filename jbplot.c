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

#include "jbplot.h"
#include "jbplot-marshallers.h"


#define USE_CAIRO 1

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


typedef struct _jbplotPrivate jbplotPrivate;

struct _jbplotPrivate
{

	gboolean dragging; /* true if the interface is being dragged */
};

enum
{
	TIME_CHANGED,
	LAST_SIGNAL
};

static guint jbplot_signals[LAST_SIGNAL] = { 0 };

static void
jbplot_class_init (jbplotClass *class)
{
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

static void
jbplot_init (jbplot *plot)
{
	gtk_widget_add_events (GTK_WIDGET (plot),
			GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
			GDK_POINTER_MOTION_MASK);

/*
	egg_clock_face_update (clock);

	// update the clock once a second 
	g_timeout_add (1000, egg_clock_face_update, clock);
*/
}

/*
static void draw_cairo (GtkWidget *plot, void *gc) {
	jbplotPrivate *priv;
	double width, height;
	int i;
	cairo_t *cr;

	cr = (cairo_t *)gc;

	priv = JBPLOT_GET_PRIVATE (plot);
	
	width = plot->allocation.width;
	height = plot->allocation.height;

	cairo_rectangle(cr, 0.1*width, 0.1*height, 0.8*width, 0.8*height);
	cairo_set_source_rgb (cr, 1, 1, 1);
	cairo_fill_preserve (cr);
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_stroke (cr);

}
*/

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



static int draw_vert_text_at_point(GtkWidget *plot, void *gc, char *text, float x, float y, anchor_t anchor) {
	static int first_time = 1;
  static PangoLayout *pl;
	int w, h;
	float x_left, y_top;
	if(first_time) {
		first_time = 0;
		pl = gtk_widget_create_pango_layout(plot, "");
	}
	pango_layout_set_text(pl, text, -1);
	pango_layout_get_size(pl, &w, &h);
	printf("w=%d, h=%d\n", w, h);
	switch(anchor) {
		case ANCHOR_TOP_LEFT:
			x_left = x;
			y_top = y;
			break;
		case ANCHOR_TOP_MIDDLE:
			x_left = x - (float)w/2/PANGO_SCALE;
			y_top = y;
			break;
		case ANCHOR_TOP_RIGHT:
			x_left = x - (float)w/PANGO_SCALE;
			y_top = y;
			break;
		case ANCHOR_MIDDLE_LEFT:
			x_left = x;
			y_top = y - (float)h/2/PANGO_SCALE;
			break;
		case ANCHOR_MIDDLE_MIDDLE:
			x_left = x - (float)w/2/PANGO_SCALE;
			y_top = y - (float)h/2/PANGO_SCALE;
			break;
		case ANCHOR_MIDDLE_RIGHT:
			x_left = x - (float)w/PANGO_SCALE;
			y_top = y - (float)h/2/PANGO_SCALE;
			break;
		case ANCHOR_BOTTOM_LEFT:
			x_left = x;
			y_top = y - (float)h/PANGO_SCALE;
			break;
		case ANCHOR_BOTTOM_MIDDLE:
			x_left = x - (float)w/2/PANGO_SCALE;
			y_top = y - (float)h/PANGO_SCALE;
			break;
		case ANCHOR_BOTTOM_RIGHT:
			x_left = x - (float)w/PANGO_SCALE;
			y_top = y - (float)h/PANGO_SCALE;
			break;
		default:
			x_left = x;
			y_top = y;
	}
	gdk_draw_layout(plot->window, gc, x_left, y_top, pl);
	return 0;
}


static int draw_horiz_text_at_point(GtkWidget *plot, void *gc, char *text, float x, float y, anchor_t anchor) {
	static int first_time = 1;
  static PangoLayout *pl;
	int w, h;
	float x_left, y_top;
	if(first_time) {
		first_time = 0;
		pl = gtk_widget_create_pango_layout(plot, "");
	}
	pango_layout_set_text(pl, text, -1);
	pango_layout_get_size(pl, &w, &h);
	printf("w=%d, h=%d\n", w, h);
	switch(anchor) {
		case ANCHOR_TOP_LEFT:
			x_left = x;
			y_top = y;
			break;
		case ANCHOR_TOP_MIDDLE:
			x_left = x - (float)w/2/PANGO_SCALE;
			y_top = y;
			break;
		case ANCHOR_TOP_RIGHT:
			x_left = x - (float)w/PANGO_SCALE;
			y_top = y;
			break;
		case ANCHOR_MIDDLE_LEFT:
			x_left = x;
			y_top = y - (float)h/2/PANGO_SCALE;
			break;
		case ANCHOR_MIDDLE_MIDDLE:
			x_left = x - (float)w/2/PANGO_SCALE;
			y_top = y - (float)h/2/PANGO_SCALE;
			break;
		case ANCHOR_MIDDLE_RIGHT:
			x_left = x - (float)w/PANGO_SCALE;
			y_top = y - (float)h/2/PANGO_SCALE;
			break;
		case ANCHOR_BOTTOM_LEFT:
			x_left = x;
			y_top = y - (float)h/PANGO_SCALE;
			break;
		case ANCHOR_BOTTOM_MIDDLE:
			x_left = x - (float)w/2/PANGO_SCALE;
			y_top = y - (float)h/PANGO_SCALE;
			break;
		case ANCHOR_BOTTOM_RIGHT:
			x_left = x - (float)w/PANGO_SCALE;
			y_top = y - (float)h/PANGO_SCALE;
			break;
		default:
			x_left = x;
			y_top = y;
	}
	gdk_draw_layout(plot->window, gc, x_left, y_top, pl);
	return 0;
}

static gboolean jbplot_expose (GtkWidget *plot, GdkEventExpose *event) {
	double width, height;

	width = plot->allocation.width;
	height = plot->allocation.height;

	GdkGC *gc;
	GdkColor fg;
	gc = plot->style->fg_gc[GTK_WIDGET_STATE (plot)];
	if(!gdk_color_parse("blue", &fg)) {
		printf("Failed parsing color by name...\n");
		fg.red   = 65535;
		fg.green = 0;
		fg.blue  = 0;
	}
	gdk_gc_set_rgb_fg_color(gc, &fg);
		
	gdk_draw_arc (plot->window, gc,
							  FALSE,
                0, 0, plot->allocation.width, plot->allocation.height,
                0, 64 * 360);

	draw_horiz_text_at_point(plot, gc, "hello there!", width/2, height/2, ANCHOR_TOP_LEFT);
	draw_vert_text_at_point(plot, gc, "hi there!", width/2, height/2, ANCHOR_TOP_LEFT);

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


GtkWidget *jbplot_new (void) {
	return g_object_new (JBPLOT_TYPE, NULL);
}
