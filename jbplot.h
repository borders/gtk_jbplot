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

G_END_DECLS

#endif
