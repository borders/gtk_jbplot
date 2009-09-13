/**
 * jbplot.h
 *
 * A GTK+ widget that implements a plot
 *
 *
 * Authors:
 *   James Borders
 */

#ifndef __JBPLOT_H__
#define __JBPLOT_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define JBPLOT_TYPE		(jbplot_get_type ())
#define JBPLOT(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), JBPLOT_TYPE, jbplot))
#define JBPLOT_CLASS(obj)	(G_TYPE_CHECK_CLASS_CAST ((obj), JBPLOT, jbplotClass))
#define IS_JPBLOT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), JBPLOT_TYPE))
#define IS_JBPLOT_CLASS(obj)	(G_TYPE_CHECK_CLASS_TYPE ((obj), JBPLOT_TYPE))
#define JBPLOT_GET_CLASS	(G_TYPE_INSTANCE_GET_CLASS ((obj), JBPLOT_TYPE, jbplotClass))


typedef struct rgb_color_t {
	float red;
	float green;
	float blue;
} rgb_color_t;


typedef enum {
	MARKER_NONE,
	MARKER_CIRCLE,
	MARKER_SQUARE,
	MARKER_X
} marker_type_t;

typedef enum {
	LINETYPE_NONE,
	LINETYPE_SOLID,
	LINETYPE_DASHED,
	LINETYPE_DOTTED
} line_type_t;

typedef struct _jbplot		jbplot;
typedef struct _jbplotClass	jbplotClass;

typedef struct trace_t *trace_handle;

struct _jbplot
{
	GtkDrawingArea parent;

	/* < private > */
};

struct _jbplotClass
{
	GtkDrawingAreaClass parent_class;

/*
	void	(* time_changed)	(EggClockFace *clock,
					 int hours, int minutes);
*/
};

GtkWidget *jbplot_new (void);
int jbplot_set_plot_title(jbplot *plot, char *title, int copy);
int jbplot_set_plot_title_visible(jbplot *plot, gboolean visible);
int jbplot_set_x_axis_label(jbplot *plot, char *title, int copy);
int jbplot_set_x_axis_label_visible(jbplot *plot, gboolean visible);
int jbplot_set_y_axis_label(jbplot *plot, char *title, int copy);
int jbplot_set_y_axis_label_visible(jbplot *plot, gboolean visible);
int jbplot_add_trace(jbplot *plot, trace_handle th);
trace_handle jbplot_create_trace(int capacity);
void jbplot_destroy_trace(trace_handle th);


int jbplot_trace_add_point(trace_handle th, float x, float y);
int jbplot_trace_set_line_props(trace_handle th, line_type_t type, float width, rgb_color_t color);
int jbplot_trace_set_marker_props(trace_handle th, marker_type_t type, float size, rgb_color_t color);


void jbplot_refresh(jbplot *plot);
int jbplot_set_x_range(jbplot *plot, float min, float max);
int jbplot_set_y_range(jbplot *plot, float min, float max);

G_END_DECLS

#endif
