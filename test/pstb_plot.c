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
#include <cairo/cairo-svg.h>

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
static int stack = 1;
static int stack_count =0;
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


void save_png(GtkButton *b, gpointer user_data) {

	GtkWidget *dialog;
	dialog = gtk_file_chooser_dialog_new ("Save File As...",
						(GtkWindow *)window,
						GTK_FILE_CHOOSER_ACTION_SAVE,
						GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
						NULL);
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER (dialog), "test.svg");
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		char *filename;
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

#if 1
		int i;
		char *per = strrchr(filename, '.');
		int base_len = per - filename;
		char *f_base = malloc(base_len + 1);
		strncpy(f_base, filename, base_len);
		f_base[base_len] = '\0';
		//printf("basename: %s\n", f_base);
		char *f = malloc(strlen(filename) + 10);
		for(i=0; i<chart_count; i++) {
			sprintf(f, "%s_%02d%s", f_base, i, per);
			jbplot_capture_svg((jbplot *)charts[i].plot, f);
		}
#else
		GdkPixmap *pm = gtk_widget_get_snapshot(v_box, NULL);
		GdkPixbuf *pb = gdk_pixbuf_get_from_drawable(NULL, pm, NULL, 0, 0, 0, 0, -1, -1);
		gdk_pixbuf_save(pb, filename, "png", NULL, NULL);
		g_object_unref(pm);
#endif

		g_free (filename);
		free(f_base);
		free(f);
	}
	gtk_widget_destroy (dialog);

}


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
		//if(p != plot) {
			jbplot_set_x_axis_scale_mode(p, SCALE_AUTO_LOOSE);
			jbplot_set_y_axis_scale_mode(p, SCALE_AUTO_LOOSE);
		//}
	}
	return 0;
}

gint pan_cb(jbplot *plot, gdouble xmin, gdouble xmax, gdouble ymin, gdouble ymax) {
	int i;
	//printf("Panning!\n");
	for(i=0; i<chart_count; i++) {
		jbplot *p = (jbplot *)(charts[i].plot);
		if(p != plot) {
			jbplot_set_x_axis_range(p, xmin, xmax);
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
	g_signal_connect(p, "pan", G_CALLBACK (pan_cb), NULL);

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
					if( (c = strtok(NULL, " \t")) == NULL || sscanf(c, "%d", &i) != 1 || 
							i < 1 || i > charts[chart_count-1].num_traces || (c = strtok(NULL, "")) == NULL) {
						printf("tracename usage: #tracename <trace_index (1-based)> <trace_name>\n");
						continue;
					}
					jbplot_trace_set_name(charts[chart_count-1].traces[i-1], c);
					jbplot_legend_refresh((jbplot *)charts[chart_count-1].plot);
				}
				else if(!strcmp(cmd,"stack")) {
					char *c = strtok(NULL, " \t");
					if(!strcmp("yes",c) || !strcmp("Yes",c) || !strcmp("YES",c) || !strcmp("1",c)) {
						stack = 1;
						printf("Stacked mode ON\n");
					}
					else if(!strcmp("no",c) || !strcmp("No",c) || !strcmp("NO",c) || !strcmp("0",c)) {
						stack = 0;
						printf("Stacked mode OFF\n");
					}
					else {
						printf("stack usage: #stack (yes|no|1|0)\n");
					}  
				}
				else if(!strcmp(cmd,"wintitle")) {
					char *c = strtok(NULL, "");
					gtk_window_set_title((GtkWindow *)window, c);
				}
				else if(!strcmp(cmd,"newplot")) {
					if(charts[chart_count-1].num_points > 0) {
						add_plot();
					}
				}
				else if(!strcmp(cmd,"xlabel")) {
					if(stack) {
						if(charts[chart_count-1].num_points < 1) {
							printf("xlabel must be run after at least one data record (when in stacked mode)\n");
							continue;
						}
						char *c;
						int i;
						if( (c = strtok(NULL, " \t")) == NULL || sscanf(c, "%d", &i) != 1 || 
								i < 1 || i > stack_count ||	(c = strtok(NULL, "")) == NULL ) {
							printf("xlabel usage: #xlabel <index (1-based)> <label_text>\n");
							continue;
						}
						jbplot_set_x_axis_label((jbplot *)charts[i - 1 + chart_count - stack_count].plot, c, 1);
					}
					else {
						char *c = strtok(NULL, "");
						jbplot_set_x_axis_label((jbplot *)charts[chart_count-1].plot, c, 1);
					}
				}
				else if(!strcmp(cmd,"ylabel")) {
					if(stack) {
						if(charts[chart_count-1].num_points < 1) {
							printf("ylabel must be run after at least one data record (when in stacked mode)\n");
							continue;
						}
						char *c;
						int i;
						if( (c = strtok(NULL, " \t")) == NULL || sscanf(c, "%d", &i) != 1 || 
								i < 1 || i > stack_count ||	(c = strtok(NULL, "")) == NULL ) {
							printf("ylabel usage: #ylabel <index (1-based)> <label_text>\n");
							continue;
						}
						jbplot_set_y_axis_label((jbplot *)charts[i - 1 + chart_count - stack_count].plot, c, 1);
					}
					else {
						char *c = strtok(NULL, "");
						jbplot_set_y_axis_label((jbplot *)charts[chart_count-1].plot, c, 1);
					}
				}
				else if(!strcmp(cmd,"title")) {
					if(stack) {
						if(charts[chart_count-1].num_points < 1) {
							printf("title must be run after at least one data record (when in stacked mode)\n");
							continue;
						}
						char *c;
						int i;
						if( (c = strtok(NULL, " \t")) == NULL || sscanf(c, "%d", &i) != 1 || 
								i < 1 || i > stack_count ||	(c = strtok(NULL, "")) == NULL ) {
							printf("title usage: #title <index (1-based)> <label_text>\n");
							continue;
						}
						jbplot_set_plot_title((jbplot *)charts[i - 1 + chart_count - stack_count].plot, c, 1);
					}
					else {
						char *c = strtok(NULL, "");
						jbplot_set_plot_title((jbplot *)charts[chart_count-1].plot, c, 1);
					}
				}
				else if(!strcmp(cmd,"xformat")) {
					char *c = strtok(NULL, " \t");
					if(c != NULL) {
						//printf("setting x format string to '%s'\n", c);
						jbplot_set_x_axis_format((jbplot *)charts[chart_count-1].plot, c);
						got_one = 1;
					}
				}
				else if(!strcmp(cmd,"yformat")) {
					char *c = strtok(NULL, " \t");
					if(c != NULL) {
						//printf("setting y format string to '%s'\n", c);
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
						printf("Incomplete data row.  Skipping...\n");
						continue;
					}

					if(stack) {
						int i;
						printf("Got %d fields (%d plots)\n", num_fields, num_fields-1);
						stack_count = num_fields - 1;

						// add more plots if needed
						if(stack_count > 1) {
							for(i=0; i<stack_count-1; i++) {
								add_plot();
							}
						}
					
						// add 1 trace per plot
						for(i=chart_count-stack_count; i<chart_count; i++) {
							trace_handle t1 = jbplot_create_trace(20000);
							if(t1==NULL) {
								printf("error creating trace!\n");
								return 0;
							}
							rgb_color_t color = {0.0, 1.0, 0.0};
							jbplot_trace_set_line_props(t1, ltypes[0], 1.0, &(colors[0]) );
							jbplot_trace_set_marker_props(t1, MARKER_CIRCLE, 1.0, &(colors[i%NUM_COLORS]));
							charts[i].traces[0] = t1;
							charts[i].num_traces = 1;
							jbplot_add_trace((jbplot *)charts[i].plot, t1);
						}
					}
					else {	
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
							jbplot_trace_set_marker_props(t1, MARKER_CIRCLE, 1.0, &(colors[i%NUM_COLORS]));
							charts[chart_count-1].traces[i] = t1;
							jbplot_add_trace((jbplot *)charts[chart_count-1].plot, t1);
						}
						jbplot_legend_refresh((jbplot *)charts[chart_count-1].plot);
					}

				}

				int n;
				char *p = line;
				int valid_fields = 0;
				double data[MAX_TRACES+1];
				while(sscanf(p, "%lf%n", data+valid_fields, &n) == 1) {
					p += n;
					valid_fields++;
					if(stack) {
						if(valid_fields > stack_count) {
							break;
						}
					}
					else {
						if(valid_fields > charts[chart_count-1].num_traces) {
							break;
						}
					}
				}

				if(stack) {
					if(valid_fields == stack_count + 1) {
						int i;
						got_one = 1;
						for(i=1; i<valid_fields; i++) {
							int ci = i - 1 + chart_count - stack_count;
							charts[ci].num_points++;
							jbplot_trace_add_point(charts[ci].traces[0], data[0], data[i]); 
						}
					}
					else {
						printf("error parsing data on line %d\n", line_count);
					}
				}
				else {
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
		}
		else if(line_index==0 && c=='#') {
			is_cmd = 1;
		}
		else {
			line[line_index++] = c;
		}
	}		

	if(got_one) {
		if(stack) {
			int i;
			for(i=chart_count-stack_count; i<chart_count; i++) {
				jbplot_refresh((jbplot *)charts[i].plot);
			}
		}
		else {
			jbplot_refresh((jbplot *)plot);
		}
	}
	//printf("** end update_data()\n");
	return TRUE;
}


void usage(char *str) {
	char *p = strrchr(str, '/');
	if(p==NULL) {
		p = str;
	}
	else {
		p++;
	}
	printf("\nUSAGE: %s [options]\n\n", p);
	printf(
"This program reads data (and optionally commands) from STDIN and displays one or \n\
more plots of the data in a separate window.  The plots are interactive, meaning \n\
that the user can zoom, pan and even track data coordinates using the mouse. \n\
\n\
INPUT FORMAT\n\
------------\n\
Data is entered using a simple whitespace-delimited text format.  The simplest example \n\
of a single (x,y) data set would look like this, with one x,y point per line: \n\
  0 0\n\
  1 2\n\
  2 4\n\
  3 6\n\
  4 8\n\
Multiple data series may be plotted too.  If the series are all the same length and also \n\
share common x values, they may be supplied as follows (see the STACK command below): \n\
  0 0 0\n\
  1 2 1\n\
  2 4 4\n\
  3 6 9\n\
  4 8 16\n\
In this case, two data sets will be plotted.  First column 1 vs column 2, and then column 1 \n\
vs column 3. \n\
\n\
COMMANDS\n\
--------\n\
Commands are denoted by a '#' character at the beginning of a line.  Commands lines may be \n\
interlaced with lines of data. A list of available commands and their usage follows:\n\
\n\
  WINTITLE\n\
    Usage: #wintitle <window_title_string> \n\
\n\
    Set the window title text.  This command can be run at any time. \n\
\n\
  NEWPLOT\n\
    Usage: #newplot \n\
\n\
    Create a new plot. \n\
\n\
  STACK\n\
    Usage: #stack <stack_mode=(yes|no|1|0)> \n\
\n\
    Set stacked plot mode. When stacked mode is ON (specified by 'yes' or '1'), multiple data \n\
    series (specified by data lines with more than 2 columns) are plotted one series per plot. \n\
    The first column is used for the x-axis of all data series in stacked mode.  With stacked \n\
    mode OFF, multiple data series are all plotted on the same graph, with each series being a \n\
    different trace on the plot and having its own legend entry.  By DEFAULT, stacked mode is ON. \n\
\n\
  XLABEL\n\
    Usage (stacked mode ON): #xlabel <plot_index> <x_axis_label_string> \n\
    Usage (stacked mode OFF): #xlabel <x_axis_label_string> \n\
\n\
    Set a plot's x-axis label.  With stacked mode OFF, this command takes only one argument \n\
    (the text itself), and can be run at any time to label the current plot.  With stacked \n\
    mode ON, the plot index (1-based) must be specified, and the command cannot be run until at \n\
    least one line of data has been read (so the program can know how many stacked plots to be \n\
    create. \n\
\n\
  YLABEL\n\
    Set a plot's y-axis label.  Similar to XLABEL. \n\
\n\
  TITLE\n\
    Set a plot's title text.  Similar to XLABEL. \n\
	\n");
	return;
}

int main (int argc, char **argv) {
	GtkWidget *top_v_box;
	GtkWidget *h_box;
	GtkWidget *save_button;

	if(argc > 1) {
		if(!strcmp(argv[1],"-h") || !strcmp(argv[1],"--help")) {
			usage(argv[0]);
			return 0;
		}
	}
	gtk_init (&argc, &argv);

	int flags;
	flags = fcntl(0, F_GETFL, 0);
  flags |= O_NONBLOCK;
  fcntl(0, F_SETFL, flags);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	top_v_box = gtk_vbox_new(FALSE, 1);
	gtk_container_add (GTK_CONTAINER (window), top_v_box);

	h_box = gtk_hbox_new(FALSE, 1);
	gtk_box_pack_start (GTK_BOX(top_v_box), h_box, FALSE, FALSE, 0);

	save_button = gtk_button_new_with_label("Save PNG");
	//gtk_box_pack_start (GTK_BOX(h_box), save_button, FALSE, FALSE, 0);
	g_signal_connect(save_button, "clicked", G_CALLBACK(save_png), NULL);

	plot_scroll_win = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_size_request(plot_scroll_win, 750, 600);
	gtk_scrolled_window_set_policy(
		GTK_SCROLLED_WINDOW(plot_scroll_win),
		GTK_POLICY_NEVER,
		GTK_POLICY_AUTOMATIC
	);
	gtk_box_pack_start (GTK_BOX(top_v_box), plot_scroll_win, TRUE, TRUE, 0);

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
