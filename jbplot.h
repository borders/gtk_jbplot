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
int jbplot_set_x_axis_label(jbplot *plot, char *title, int copy);
int jbplot_set_y_axis_label(jbplot *plot, char *title, int copy);
//int jbplot_add_trace(jbplot *plot, float *x, float *y, int length, int capacity);
int jbplot_add_trace(jbplot *plot, trace_handle th);
trace_handle jbplot_create_trace(int capacity);
void jbplot_destroy_trace(trace_handle th);
int jbplot_trace_add_point(trace_handle th, float x, float y);
void jbplot_refresh(jbplot *plot);

G_END_DECLS

#endif
