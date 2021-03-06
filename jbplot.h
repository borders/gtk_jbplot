/** \file jbplot.h
 * \brief This file describes the public interface to the jbplot widget
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

/**
 * Supported crosshair modes
 */
typedef enum {
	CROSSHAIR_NONE,
	CROSSHAIR_FREE,
	CROSSHAIR_SNAP
} crosshair_t;

/**
 * Type used to specify colors
 */
typedef struct rgb_color_t {
	double red;
	double green;
	double blue;
} rgb_color_t;

/**
 * Supported plot area margin mode
 */
typedef enum {
	MARGIN_AUTO,
	MARGIN_PX,
	MARGIN_PERCENT
} margin_mode_t;


/**
 * Supported axis scaling modes
 */
typedef enum {
	SCALE_AUTO_TIGHT,
	SCALE_AUTO_LOOSE,
	SCALE_MANUAL
} scale_mode_t;


/**
 * Supported trace marker types
 */
typedef enum {
	MARKER_NONE,
	MARKER_CIRCLE,
	MARKER_SQUARE,
	MARKER_X,
	MARKER_POINT
} marker_type_t;

/**
 * Supported cursor types
 */
typedef enum {
	CURSOR_NONE,
	CURSOR_VERT,
	CURSOR_HORIZ,
	CURSOR_CROSS,
} cursor_type_t;


/**
 * Supported trace line types
 */
typedef enum {
	LINETYPE_NONE,
	LINETYPE_SOLID,
	LINETYPE_DASHED,
	LINETYPE_DOTTED
} line_type_t;

/**
 * Supported legend positions
 */
typedef enum {
	LEGEND_POS_NONE,
	LEGEND_POS_RIGHT,
	LEGEND_POS_TOP
} legend_pos_t;

typedef struct _jbplot		jbplot;
typedef struct _jbplotClass	jbplotClass;

typedef struct trace_t *trace_handle;

struct _jbplot
{
	GtkDrawingArea parent;

};

struct _jbplotClass
{
	GtkDrawingAreaClass parent_class;

	void (* zoom_in) (jbplot *plot, gdouble xmin, gdouble xmax, gdouble ymin, gdouble ymax);
	void (* zoom_all) (jbplot *plot);
};


GtkWidget *jbplot_new (void);

void jbplot_destroy(jbplot *plot);

int jbplot_set_plot_title(jbplot *plot, char *title, int copy);

int jbplot_set_plot_title_visible(jbplot *plot, gboolean visible);

int jbplot_set_bg_color(jbplot *plot, rgb_color_t *color);

int jbplot_clear_data(jbplot *p);
int jbplot_trace_clear_data(trace_handle th);

/********************** X-axis functions *********************************************/
int jbplot_set_x_axis_tics(jbplot *plot, int n, double *values, char **labels);
int jbplot_set_x_axis_format(jbplot *plot, char *str);
int jbplot_set_x_axis_label(jbplot *plot, char *title, int copy);
char *jbplot_get_x_axis_label(jbplot *plot);
int jbplot_set_x_axis_label_visible(jbplot *plot, gboolean visible);
int jbplot_set_x_axis_range(jbplot *plot, double min, double max, int history);
int jbplot_get_x_axis_range(jbplot *plot, double *min, double *max);
int jbplot_set_x_axis_scale_mode(jbplot *plot, scale_mode_t mode, int history);
int jbplot_set_x_axis_gridline_props(jbplot *plot, line_type_t type, double width, rgb_color_t *color);
int jbplot_set_x_axis_gridline_visible(jbplot *plot, gboolean visible);


/********************* Y-axis functions *******************************************/
int jbplot_set_y_axis_tics(jbplot *plot, int n, double *values, char **labels);
int jbplot_set_y_axis_format(jbplot *plot, char *str);
int jbplot_set_y_axis_label(jbplot *plot, char *title, int copy);
char *jbplot_get_y_axis_label(jbplot *plot);
int jbplot_set_y_axis_label_visible(jbplot *plot, gboolean visible);
int jbplot_set_y_axis_range(jbplot *plot, double min, double max, int history);
int jbplot_get_y_axis_range(jbplot *plot, double *min, double *max);
int jbplot_set_y_axis_scale_mode(jbplot *plot, scale_mode_t mode, int history);
int jbplot_set_y_axis_gridline_props(jbplot *plot, line_type_t type, double width, rgb_color_t *color);
int jbplot_set_y_axis_gridline_visible(jbplot *plot, gboolean visible);

/* Plot-Area functions */
int jbplot_set_coords_visible(jbplot *plot, int vis);
int jbplot_set_crosshair_mode(jbplot *plot, crosshair_t mode);
int jbplot_set_plot_area_color(jbplot *plot, rgb_color_t *color);
int jbplot_set_plot_area_border(jbplot *plot, double width, rgb_color_t *color);
int jbplot_set_plot_area_LR_margins(jbplot *plot, margin_mode_t mode, double left, double right);
int jbplot_get_ideal_LR_margins(jbplot *plot, double *left, double *right);

int jbplot_set_legend_props(jbplot *plot, double border_width, rgb_color_t *bg_color, rgb_color_t *border_color, legend_pos_t position);
int jbplot_legend_refresh(jbplot *plot);

int jbplot_undo_zoom(jbplot *plot);
int jbplot_set_xy_range(jbplot *plot, double xmin, double xmax, double ymin, double ymax, int history);
int jbplot_set_xy_scale_mode(jbplot *plot, scale_mode_t mode, int history);

/* Trace-related functions */
int jbplot_trace_resize(trace_handle th, int new_size);
int jbplot_add_trace(jbplot *plot, trace_handle th);
int jbplot_remove_trace(jbplot *plot, trace_handle th);
trace_handle jbplot_create_trace(int capacity);
void jbplot_destroy_trace(trace_handle th);
int jbplot_trace_set_data(trace_handle th, double *x_start, double *y_start, int length);
int jbplot_trace_get_data(trace_handle th, double **x, double **y, int *length);
int jbplot_trace_set_decimation(trace_handle th, int divisor);

trace_handle jbplot_create_trace_with_external_data(double *x, double *y, int length, int capacity);
int jbplot_trace_add_point(trace_handle th, double x, double y);
int jbplot_trace_set_line_props(trace_handle th, line_type_t type, double width, rgb_color_t *color);
int jbplot_trace_set_marker_props(trace_handle th, marker_type_t type, double size, rgb_color_t *color);
int jbplot_trace_set_name(trace_handle th, char *name);
char *jbplot_trace_get_name(trace_handle th);
int jbplot_trace_clear_data(trace_handle th);

trace_handle *jbplot_get_traces(jbplot *plot);
int jbplot_get_trace_count(jbplot *plot);

/* cursor related functions */
int jbplot_set_cursor_pos(jbplot *plot, double x, double y);
int jbplot_set_cursor_props(
	jbplot *plot, 
	cursor_type_t type, 
	rgb_color_t color,
	double line_width, 
	int line_type);

int jbplot_capture_png(jbplot *plot, char *filename, int width, int height);
int jbplot_capture_svg(jbplot *plot, char *filename);

void jbplot_refresh(jbplot *plot);
int jbplot_set_antialias(jbplot *plot, gboolean state);

G_END_DECLS

#endif
