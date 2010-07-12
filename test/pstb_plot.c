/**
 * main.c
 */

#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "../jbplot.h"

#define MAX_CHARTS 10
#define MAX_TRACES 10


rgb_color_t colors[] = {
	{0.0, 0.0, 0.0},
	{1.0, 0.0, 0.0},
	{1.0, 1.0, 0.0},
	{0.0, 1.0, 0.0},
	{0.0, 1.0, 1.0},
	{0.0, 0.0, 1.0},
	{1.0, 0.0, 1.0}
};
#define NUM_COLORS 7

line_type_t ltypes[] = {
	LINETYPE_SOLID,
	LINETYPE_DASHED,
	LINETYPE_DOTTED
};
#define NUM_LTYPES 3

struct chart {
	GtkWidget *plot;
	int num_points;
	trace_handle traces[MAX_TRACES];
	int num_traces;
};

static int t = 0;
static trace_handle t1;
static GtkWidget *window;
static GtkWidget *v_box;
static GtkWidget *plot_scroll_win;
static struct chart charts[MAX_CHARTS];
static int chart_count = 0;
static int run = 1;
#define LINE_SIZE 1000
static char line[LINE_SIZE];
static char line_index = 0;
static int line_count = 0;
static int is_cmd = 0;


gint zoom_in_cb(jbplot *plot, gdouble xmin, gdouble xmax, gdouble ymin, gdouble ymax) {
	int i;
	printf("Zoomed In!\n");
	printf("x-range: (%lg, %lg)\n", xmin, xmax);
	printf("y-range: (%lg, %lg)\n", ymin, ymax);
	for(i=0; i<chart_count; i++) {
		jbplot *p = (jbplot *)(charts[i].plot);
		if(p != plot) {
			jbplot_set_x_axis_range(p, xmin, xmax);
		}
	}
	return 0;
}


gint zoom_all_cb(jbplot *plot) {
	int i;
	printf("Zoom All!\n");
	for(i=0; i<chart_count; i++) {
		jbplot *p = (jbplot *)(charts[i].plot);
		if(p != plot) {
			jbplot_set_x_axis_scale_mode(p, SCALE_AUTO_TIGHT);
			jbplot_set_y_axis_scale_mode(p, SCALE_AUTO_TIGHT);
		}
	}
	return 0;
}


char *strip_leading_ws(char *s) {
	int i=0;
	while(isspace(s[i])) {
		i++;
	}
	return s+i;
}


static int add_plot() {
	if(chart_count >= MAX_CHARTS) {
		return -1;
	}
	GtkWidget *p = jbplot_new ();
	gtk_widget_set_size_request(p, 700, 125);
	gtk_box_pack_start (GTK_BOX(v_box), p, TRUE, TRUE, 0);

	g_signal_connect(p, "zoom-in", G_CALLBACK (zoom_in_cb), NULL);
	g_signal_connect(p, "zoom-all", G_CALLBACK (zoom_all_cb), NULL);

	//jbplot_set_plot_title((jbplot *)p, "PSTB plot", 1);
	jbplot_set_plot_title_visible((jbplot *)p, 0);
	jbplot_set_x_axis_label((jbplot *)p, "Time (sec)", 1);
	jbplot_set_x_axis_label_visible((jbplot *)p, 1);
	jbplot_set_y_axis_label((jbplot *)p, "Amplitude", 1);
	jbplot_set_y_axis_label_visible((jbplot *)p, 1);

	jbplot_set_plot_area_LR_margins((jbplot *)p, MARGIN_PX, 75, 100);

	//jbplot_set_x_axis_format((jbplot *)p, "%.0f");

	jbplot_set_legend_props((jbplot *)p, 1.0, NULL, NULL, LEGEND_POS_RIGHT);
	jbplot_legend_refresh((jbplot *)p);

	rgb_color_t gridline_color = {0.7, 0.7, 0.7};
	jbplot_set_x_axis_gridline_props((jbplot *)p, LINETYPE_DASHED, 1.0, &gridline_color);
	jbplot_set_y_axis_gridline_props((jbplot *)p, LINETYPE_DASHED, 1.0, &gridline_color);

	charts[chart_count].plot = p;
	charts[chart_count].num_points = 0;
	charts[chart_count].num_traces = 0;
	chart_count++;
	gtk_widget_show(p);
	return 0;
}


gboolean update_data(gpointer data) {
	int i;
	int ret;
	char c;
	int got_one = 0;
	//printf("** start update_data()\n");
	if(!run) {
		return TRUE;
	}

	GtkWidget *plot = charts[chart_count-1].plot;
	while((ret = read(0, &c, 1)) > 0) {
		//printf("got char: %c\n", c);
		if(c == '\n') {
			line[line_index] = '\0';
			//printf("got line: '%s'\n", line);
			line_count++;
			line_index = 0;
			if(is_cmd) {
				is_cmd = 0;
				char *cmd;
				cmd = strtok(strip_leading_ws(line), " \t"); 
				printf("Got command: %s\n", cmd);
				if(!strcmp(cmd,"tracename")) {
					char *c;
					int i;
					if(charts[chart_count-1].num_points < 1) {
						printf("tracename must be run after at least one data record\n");
						continue;
					}
					if( (c = strtok(NULL, " \t")) == NULL || sscanf(c, "%d", &i) != 1 || (c = strtok(NULL, "")) == NULL) {
						printf("tracename usage: #tracename <trace_index> <trace_name>\n");
						continue;
					}
					jbplot_trace_set_name(charts[chart_count-1].traces[i], c);
					jbplot_legend_refresh((jbplot *)charts[chart_count-1].plot);
				}
				else if(!strcmp(cmd,"wintitle")) {
					gtk_window_set_title((GtkWindow *)window, cmd+9);
				}
				else if(!strcmp(cmd,"newplot")) {
					if(charts[chart_count-1].num_points > 0) {
						add_plot();
					}
				}
				else if(!strcmp(cmd,"xlabel")) {
					//printf("setting xlabel to '%s'\n", cmd+7);
					jbplot_set_x_axis_label((jbplot *)charts[chart_count-1].plot, cmd+7, 1);
				}
				else if(!strcmp(cmd,"ylabel")) {
					jbplot_set_y_axis_label((jbplot *)charts[chart_count-1].plot, cmd+7, 1);
				}
				else if(!strcmp(cmd,"title")) {
					jbplot_set_plot_title((jbplot *)charts[chart_count-1].plot, cmd+6, 1);
				}
				else if(!strcmp(cmd,"xformat")) {
					char *c = strtok(NULL, " \t");
					if(c != NULL) {
						printf("setting x format string to '%s'\n", c);
						jbplot_set_x_axis_format((jbplot *)charts[chart_count-1].plot, c);
						got_one = 1;
					}
				}
				else if(!strcmp(cmd,"yformat")) {
					char *c = strtok(NULL, " \t");
					if(c != NULL) {
						printf("setting y format string to '%s'\n", c);
						jbplot_set_y_axis_format((jbplot *)charts[chart_count-1].plot, c);
						got_one = 1;
					}
				}
			}
			else {
				// first data record for this chart?
				if(charts[chart_count-1].num_points == 0) {
					// count up the number of columns in this row (i.e. # of traces)
					double d;
					int n;
					int num_fields = 0;
					char *p = line;
					while(sscanf(p, "%lf%n", &d, &n) == 1) {
						p += n;
						num_fields++;
						if(num_fields > MAX_TRACES) {
							break;
						}
					}
					if(num_fields < 2) {
						continue;
					}			
					printf("Got %d fields (%d traces)\n", num_fields, num_fields-1);
					// first column is shared among all traces
					charts[chart_count-1].num_traces = num_fields - 1;

					// create the traces
					int i;
					for(i=0; i<num_fields-1; i++) {
						trace_handle t1 = jbplot_create_trace(20000);
						if(t1==NULL) {
							printf("error creating trace!\n");
							return 0;
						}
						rgb_color_t color = {0.0, 1.0, 0.0};
						jbplot_trace_set_line_props(t1, ltypes[i%NUM_LTYPES], 1.0, &(colors[i%NUM_COLORS]) );
						color.red = 1.0; color.green = 0.0;	color.blue = 0.0;
						color.red = 0.0; color.green = 0.0;	color.blue = 1.0;
						//jbplot_trace_set_marker_props(t1, MARKER_CIRCLE, 2.0, &color);
						charts[chart_count-1].traces[i] = t1;
						jbplot_add_trace((jbplot *)charts[chart_count-1].plot, t1);
					}
					jbplot_legend_refresh((jbplot *)charts[chart_count-1].plot);

				}

				int n;
				char *p = line;
				int valid_fields = 0;
				double data[MAX_TRACES+1];
				while(sscanf(p, "%lf%n", data+valid_fields, &n) == 1) {
					p += n;
					valid_fields++;
					if(valid_fields > charts[chart_count-1].num_traces) {
						break;
					}
				}
				if(valid_fields == charts[chart_count-1].num_traces + 1) {
					got_one = 1;
					charts[chart_count-1].num_points++;
					int i;
					for(i=1; i<valid_fields; i++) {
						//printf("adding point: (%lg, %lg)\n", data[0], data[i]); 
						jbplot_trace_add_point(charts[chart_count-1].traces[i-1], data[0], data[i]); 
					}
				}
				else {
					printf("error parsing data on line %d\n", line_count);
				}

			}
		}
		else if(line_index==0 && c=='#') {
			is_cmd = 1;
		}
		else {
			line[line_index++] = c;
		}
	}		

	if(got_one) {
		jbplot_refresh((jbplot *)plot);
	}
	//printf("** end update_data()\n");
	return TRUE;
}

int main (int argc, char **argv) {
	GtkWidget *button;

	gtk_init (&argc, &argv);

	int flags;
	flags = fcntl(0, F_GETFL, 0);
  flags |= O_NONBLOCK;
  fcntl(0, F_SETFL, flags);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	plot_scroll_win = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_size_request(plot_scroll_win, 700, 600);
	gtk_scrolled_window_set_policy(
		GTK_SCROLLED_WINDOW(plot_scroll_win),
		GTK_POLICY_NEVER,
		GTK_POLICY_AUTOMATIC
	);
	gtk_container_add (GTK_CONTAINER (window), plot_scroll_win);

	v_box = gtk_vbox_new(FALSE, 1);
	gtk_scrolled_window_add_with_viewport(
		GTK_SCROLLED_WINDOW(plot_scroll_win),
		v_box
	);

	add_plot();

	gtk_widget_show_all(window);

	g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

	g_timeout_add(50, update_data, t1);

	gtk_main ();

	return 0;
}
