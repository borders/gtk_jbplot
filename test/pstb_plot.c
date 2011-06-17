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
#include <ctype.h>
#include <stdarg.h>
#include <cairo/cairo-svg.h>

#include "../jbplot.h"

#define MAX_CHARTS 20
#define MAX_TRACES 20


rgb_color_t colors[] = {
	{0.0, 0.0, 0.0}, // black
	{1.0, 0.0, 0.0}, // red
	{0.0, 1.0, 0.0}, // green
	{0.0, 0.0, 1.0}, // blue
	{1.0, 0.0, 1.0}  // pink
};
#define NUM_COLORS 5

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

static char *cmd_line_png_fname = NULL;
static int cmd_line_png = 0;
static int save_count = 1;
static int lmargin = 75;
static int rmargin = 110;
static int t = 0;
static int num_fields = 0;
static int stack = 1;
static int stack_count =0;
static GtkWidget *cursor_button;
static GtkWidget *coords_button;
static GtkWidget *snap_button;
static GtkWidget *clear_all_button;
static GtkWidget *window;
static GtkWidget *v_box;
static GtkWidget *plot_scroll_win;
static struct chart charts[MAX_CHARTS];
static int chart_count = 0;
#define LINE_SIZE 50000
static char line[LINE_SIZE];
static int line_index = 0;
static int line_count = 0;
static int is_cmd = 0;
static int quiet = 0;
static int needs_lineup = 1;

void myprintf(const char *fmt, ...) {
	if(!quiet) {
		va_list arg;
		va_start(arg, fmt);
		vprintf(fmt, arg);
		va_end(arg);
	}
	return;
}


int  lineup_margins(void) {
	int i;
	double max_left = 0.0;
	double max_right = 0.0;
	int valid = 1;
	for(i=0; i<chart_count; i++) {
		double left, right;
		jbplot *p = (jbplot *)(charts[i].plot);
		if(jbplot_get_ideal_LR_margins(p, &left, &right) < 0) {
			valid = 0;
			break;
		}
		myprintf("left: %g; right: %g\n", left, right);
		if(left > max_left) max_left = left;
		if(right > max_right) max_right = right;
	}
	if(!valid) {
		myprintf("invalid ideal margins\n");
		return -1;
	}
	myprintf("max_left: %g; max_right: %g\n", max_left, max_right);
	for(i=0; i<chart_count; i++) {
		jbplot *p = (jbplot *)(charts[i].plot);
		jbplot_set_plot_area_LR_margins(p, MARGIN_PX, max_left, max_right);
	}
	return 0;
}


void cursor_cb(GtkToggleButton *b, gpointer user_data) {
	int i;
	int state = gtk_toggle_button_get_active(b);
	int snap = gtk_toggle_button_get_active((GtkToggleButton *)snap_button);
	for(i=0; i<chart_count; i++) {
		jbplot *p = (jbplot *)(charts[i].plot);
		if(state) {
			if(snap) {
				jbplot_set_crosshair_mode(p, CROSSHAIR_SNAP);
			}
			else {
				jbplot_set_crosshair_mode(p, CROSSHAIR_FREE);
			}
		}
		else {
			jbplot_set_crosshair_mode(p, CROSSHAIR_NONE);
		}
	}
}

void coords_cb(GtkToggleButton *b, gpointer user_data) {
	int i;
	int state = gtk_toggle_button_get_active(b);
	for(i=0; i<chart_count; i++) {
		jbplot *p = (jbplot *)(charts[i].plot);
		if(state) {
			jbplot_set_coords_visible(p, 1);
		}
		else {
			jbplot_set_coords_visible(p, 0);
		}
	}
}

void snap_cb(GtkToggleButton *b, gpointer user_data) {
	int i;
	int state = gtk_toggle_button_get_active(b);
	int ch = gtk_toggle_button_get_active((GtkToggleButton *)cursor_button);
	for(i=0; i<chart_count; i++) {
		jbplot *p = (jbplot *)(charts[i].plot);
		if(state) {
			jbplot_set_crosshair_mode(p, CROSSHAIR_SNAP);
			if(ch == CROSSHAIR_NONE) {
				jbplot_set_crosshair_mode(p, CROSSHAIR_NONE);
			}
		}
		else {
			jbplot_set_crosshair_mode(p, CROSSHAIR_FREE);
			if(ch == CROSSHAIR_NONE) {
				jbplot_set_crosshair_mode(p, CROSSHAIR_NONE);
			}
		}
	}
}

int save_png(char *filename) {
	int i;
	char *per = strrchr(filename, '.');
	int fl = strlen(filename);
	if(fl < 5 || strcmp(filename+fl-1-3,".png")) {	
		printf("Filename must have \".png\" extension\n");
		return -1;
	}
	int w=0, h=0;
	if(cmd_line_png) {
		w = 900;
		h = 125;
	}
	int base_len = per - filename;
	char *f_base = malloc(base_len + 1);
	strncpy(f_base, filename, base_len);
	f_base[base_len] = '\0';
	//myprintf("basename: %s\n", f_base);
	char *f = malloc(strlen(filename) + 10);
	char *cmd = malloc(1000 + chart_count * (strlen(filename)+10));
	cmd[0] = '\0';
	strcat(cmd, "montage -mode concatenate -tile 1x ");
	for(i=0; i<chart_count; i++) {
		sprintf(f, "%s_%02d%s", f_base, i, per);
		jbplot_capture_png((jbplot *)charts[i].plot, f, w, h);
		strcat(cmd, f);
		strcat(cmd, " ");
	}
	strcat(cmd, filename);
	myprintf("executing: \"%s\"\n", cmd);
	system(cmd);

	char cmd2[1000];
	char title_fname[1000];
	sprintf(title_fname, "%s_title.png", f_base);
	char pic_title[] = "Test_Title";
	sprintf(cmd2, "convert -size %dx%d -gravity center label:\"%s\" \"%s\"", 600, 15, filename, title_fname);
	myprintf("executing: \"%s\"\n", cmd2);
	system(cmd2);
	char cmd3[1000];
	sprintf(cmd3, "montage -mode concatenate -tile 1x \"%s\" \"%s\" \"%s\"", title_fname, filename, filename);
					
	myprintf("executing: \"%s\"\n", cmd3);
	system(cmd3);
	remove(title_fname);

	for(i=0; i<chart_count; i++) {
		sprintf(f, "%s_%02d%s", f_base, i, per);
		remove(f);
	}
	free(f_base);
	free(f);
	free(cmd);
	return 0;
}

int is_valid_fname(char *str) {
	return (strpbrk(str, "()&") == NULL);
}

void clear_cb(GtkButton *b, gpointer user_data) {
	int i;
	for(i=0; i<chart_count; i++) {
		jbplot_clear_data((jbplot *)(charts[i].plot));
	}
}

void save_png_cb(GtkButton *b, gpointer user_data) {
	GtkWidget *dialog;
	dialog = gtk_file_chooser_dialog_new ("Save File As...",
						(GtkWindow *)window,
						GTK_FILE_CHOOSER_ACTION_SAVE,
						GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
						NULL);
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
	const char *wt = gtk_window_get_title((GtkWindow *)window);
	char *default_fname;
	if(wt!=NULL && strlen(wt) > 0) {
		default_fname = malloc(strlen(wt) + 1000);
		default_fname[0] = '\0';
		strcpy(default_fname, wt);
		/* replace all whitespace chars with underscores */
		int i;
		for(i=0; i<strlen(default_fname); i++) {
			if(isspace(default_fname[i])) {
				default_fname[i] = '_';
			}
		}
		sprintf(default_fname+strlen(default_fname), "_plot%02d.png", save_count);
	}
	else {
		default_fname = malloc(100);
		sprintf(default_fname, "plot%02d.png", save_count);
	}
	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER (dialog), default_fname);
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		char *filename;
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		if(is_valid_fname(filename)) {
			save_png(filename);
			//printf("filename: '%s'\n", filename);
			//printf("default filename: '%s'\n", default_fname);
			if(strstr(filename, default_fname) != NULL) {
				save_count++;
			}
		}
		else {
			printf("Invalid filename: %s\n", filename);
		}
		g_free (filename);
	}
	free(default_fname);
	gtk_widget_destroy (dialog);

}


gint zoom_in_cb(jbplot *plot, gdouble xmin, gdouble xmax, gdouble ymin, gdouble ymax) {
	int i;
	myprintf("Zoomed In!\n");
	myprintf("x-range: (%lg, %lg)\n", xmin, xmax);
	myprintf("y-range: (%lg, %lg)\n", ymin, ymax);
	for(i=0; i<chart_count; i++) {
		jbplot *p = (jbplot *)(charts[i].plot);
		if(p != plot) {
			jbplot_set_x_axis_range(p, xmin, xmax);
		}
	}
	needs_lineup = 1;
	return 0;
}


gint zoom_all_cb(jbplot *plot) {
	int i;
	myprintf("Zoom All!\n");
	for(i=0; i<chart_count; i++) {
		jbplot *p = (jbplot *)(charts[i].plot);
		//if(p != plot) {
			jbplot_set_x_axis_scale_mode(p, SCALE_AUTO_TIGHT);
			jbplot_set_y_axis_scale_mode(p, SCALE_AUTO_TIGHT);
		//}
	}
	needs_lineup = 1;
	return 0;
}

gint pan_cb(jbplot *plot, gdouble xmin, gdouble xmax, gdouble ymin, gdouble ymax) {
	int i;
	//myprintf("Panning!\n");
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
	//printf("adding plot\n");
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

	jbplot_set_plot_area_LR_margins((jbplot *)p, MARGIN_PX, lmargin, rmargin);

	//jbplot_set_x_axis_format((jbplot *)p, "%.0f");

	if(stack) {
		jbplot_set_legend_props((jbplot *)p, 1.0, NULL, NULL, LEGEND_POS_NONE);
		jbplot_legend_refresh((jbplot *)p);
	}
	else {
		jbplot_set_legend_props((jbplot *)p, 1.0, NULL, NULL, LEGEND_POS_RIGHT);
		jbplot_legend_refresh((jbplot *)p);
	}

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

int add_trace(struct chart *chart) {
	if(chart->num_traces >= MAX_TRACES) {
		printf("Max number of traces (%d) exceeded.  Exiting...\n", MAX_TRACES);
		exit(1);
	}
	int i = chart->num_traces;
	trace_handle t1 = jbplot_create_trace(500000);
	if(t1==NULL) {
		printf("error creating trace!\n");
		return 0;
	}
	jbplot_trace_set_line_props(t1, ltypes[(i/NUM_COLORS)%NUM_LTYPES], 1.0, &(colors[i%NUM_COLORS]) );
	jbplot_trace_set_marker_props(t1, MARKER_CIRCLE, 2.0, &(colors[i%NUM_COLORS]));
	chart->traces[i] = t1;
	jbplot_add_trace((jbplot *)chart->plot, t1);
	jbplot_legend_refresh((jbplot *)charts->plot);
	chart->num_traces++;
	return 0;
}


char *strtolower(char *str) {
	static char s[255];
	int len = strlen(str);
	if(len >= sizeof(s)) {
		return NULL;
	}
	int i;
	for(i=0; i<len; i++) {
		s[i] = tolower(str[i]);
	}
	s[i] = '\0';
	return s;
}

gboolean update_data(gpointer data) {
	int i;
	int ret;
	char c;
	int got_one = 0;

	while((ret = read(0, &c, 1)) > 0) {
		//myprintf("got char: %c\n", c);
		if(c == '\n') {
			line[line_index] = '\0';
			//myprintf("got line: '%s'\n", line);
			line_count++;
			line_index = 0;
			if(is_cmd) {
				is_cmd = 0;
				char *cmd;
				cmd = strtok(strip_leading_ws(line), " \t"); 
				//myprintf("Got command: %s\n", cmd);
				if(!strcmp(cmd,"tracename")) {
					char *c;
					int i;
					if(charts[chart_count-1].num_points < 1) {
						myprintf("tracename must be run after at least one data record\n");
						continue;
					}
					if( (c = strtok(NULL, " \t")) == NULL || sscanf(c, "%d", &i) != 1 || 
							i < 1 || i > charts[chart_count-1].num_traces || (c = strtok(NULL, "")) == NULL) {
						myprintf("tracename usage: #tracename <trace_index (1-based)> <trace_name>\n");
						continue;
					}
					jbplot_trace_set_name(charts[chart_count-1].traces[i-1], c);
					jbplot_legend_refresh((jbplot *)charts[chart_count-1].plot);
					myprintf("Setting name of trace %d to '%s'\n", i, c);
				}
				else if(!strcmp(cmd,"traceprops")) {
					if(stack) {
						if(charts[chart_count-1].num_points < 1) {
							myprintf("traceprops must be run after at least one data record (when in stacked mode)\n");
							continue;
						}
						char *c;
						int plot_index;
						int trace_index;
						int lt, col;
						float lw, ms;
						if(  (c=strtok(NULL,"")) == NULL || 
								 sscanf(c,"%d %d %d %f %f %d",&plot_index,&trace_index,&lt,&lw,&ms,&col) != 6 ||
						     plot_index < 1 || plot_index > chart_count ||
								 trace_index < 1 || trace_index > charts[chart_count-1].num_traces ||
								 lt >= NUM_LTYPES || lw < 0.f || ms < 0.f || col < 0 || col >= NUM_COLORS) {
							myprintf("%s usage: #%s <plot_index> <trace_index> <line_type> <line_width> <marker_size> <color>\n", cmd, cmd);
							continue;
						}
						jbplot_trace_set_line_props(charts[plot_index-1].traces[trace_index-1], (lt<0)?LINETYPE_NONE:ltypes[lt], lw, &(colors[col]) );
						jbplot_trace_set_marker_props(charts[plot_index-1].traces[trace_index-1], MARKER_CIRCLE, ms, &(colors[col]));
						jbplot_legend_refresh((jbplot *)charts[plot_index-1].plot);
						got_one = 1;
					}
					else {
						if(charts[chart_count-1].num_points < 1) {
							myprintf("%s must be run after at least one data record\n", cmd);
							continue;
						}
						char *c;
						int i;
						int lt, col;
						float lw, ms;
						if(  (c=strtok(NULL,"")) == NULL || 
								 sscanf(c,"%d %d %f %f %d",&i,&lt,&lw,&ms,&col) != 5 ||
								 i < 1 || i > charts[chart_count-1].num_traces ||
								 lt >= NUM_LTYPES || lw < 0.f || ms < 0.f || col < 0 || col >= NUM_COLORS) {
							myprintf("%s usage: #%s <trace_index> <line_type> <line_width> <marker_size> <color>\n", cmd, cmd);
							continue;
						}
						jbplot_trace_set_line_props(charts[chart_count-1].traces[i-1], (lt<0)?LINETYPE_NONE:ltypes[lt], lw, &(colors[col]) );
						jbplot_trace_set_marker_props(charts[chart_count-1].traces[i-1], MARKER_CIRCLE, ms, &(colors[col]));
						jbplot_legend_refresh((jbplot *)charts[chart_count-1].plot);
						got_one = 1;
					}
				}
				else if(!strcmp(cmd,"stack")) {
					char *usage = "stack usage: #stack (yes|no|1|0|on|off)\n";
					char *c = strtok(NULL, " \t");
					if(c != NULL) {
						char *d = strtolower(c);
						if(!strcmp("yes",d) || !strcmp("1",d) || !strcmp("on",d)) {
							stack = 1;
							myprintf("Stacked mode ON\n");
						}
						else if(!strcmp("no",d) || !strcmp("0",d) || !strcmp("off",d)) {
							stack = 0;
							// special case: make legend visible on first chart
							if(chart_count==1 && charts[0].num_points == 0) {
								jbplot_set_legend_props((jbplot *)charts[0].plot, 1.0, NULL, NULL, LEGEND_POS_RIGHT);
								jbplot_legend_refresh((jbplot *)charts[0].plot);
							}
							myprintf("Stacked mode OFF\n");
						}
						else {
							myprintf(usage);
						}  
					}
					else {
						myprintf(usage);
					}

				}
				else if(!strcmp(cmd,"wintitle")) {
					char *c = strtok(NULL, "");
					gtk_window_set_title((GtkWindow *)window, c);
					myprintf("Setting window title to '%s'\n", c);
				}
				else if(!strcmp(cmd,"newplot")) {
					if(charts[chart_count-1].num_points > 0) {
						add_plot();
						myprintf("Creating new plot\n");
					}
				}
				else if(!strcmp(cmd,"newtrace")) {
					if(stack) {
						myprintf("#newtrace not supported in stacked mode\n");
					}
					else {
						// setting num_points to zero will create a new trace(s) upon next valid data row
						charts[chart_count-1].num_points = 0;
						myprintf("Creating new trace(s)\n");
					}
				}
				else if(!strcmp(cmd,"xmargins")) {
					if(stack) {
						myprintf("#xmargins command not currently supported in stacked mode\n");
						continue;
					}
					else {
						char *usage = "xmargins usage: #xmargins <left_px> <right_px>\n";
						char *dat;
						if( (dat = strtok(NULL, "")) == NULL) {
							myprintf(usage);
							continue;
						}
						if(sscanf(dat, "%d %d", &lmargin, &rmargin) != 2) {
							myprintf(usage);
							continue;
						}
						int i;
						for(i=0; i<chart_count; i++) {
							jbplot_set_plot_area_LR_margins((jbplot *)charts[i].plot, MARGIN_PX, lmargin, rmargin);
						}
					}
				}
				else if(!strcmp(cmd,"xtics")) {
					if(stack) {
						myprintf("#xtics command not currently supported in stacked mode\n");
						continue;
					}
					else {
						char *dat;
						if( (dat = strtok(NULL, "")) == NULL) {
							myprintf("xtics usage: #xtics <val1> <label1> <val2> <label2> ...\n");
							continue;
						}
						double values[20];
						int i;
						char *labels[20];
						for(i=0; i<20; i++) {
							labels[i] = malloc(150);
						}
						for(i=0; i<20; i++) {
							int nr;
							if(sscanf(dat, "%lf %s%n", &(values[i]), labels[i], &nr) != 2) {
								break;
							}
							myprintf("%g, %s\n", values[i], labels[i]);
							dat = dat + nr;
						}
						if(i >= 2) {
							jbplot_set_x_axis_tics((jbplot *)charts[chart_count-1].plot,i,values,labels);
							myprintf("setting x tics listed above...\n");
						}
						for(i=0; i<20; i++) {
							free(labels[i]);
						}
					}
				}
				else if(!strcmp(cmd,"ytics")) {
					char *dat;
					int chart_index;
					if(stack) {
						if(charts[chart_count-1].num_points < 1) {
							myprintf("ylabel must be run after at least one data record (when in stacked mode)\n");
							continue;
						}
						char *c;
						int i;
						if( (c = strtok(NULL, " \t")) == NULL || sscanf(c, "%d", &i) != 1 || 
								i < 1 || i > stack_count || (dat=strtok(NULL,"")) == NULL) {
							myprintf("ytics usage: #ylabel <index (1-based)> <val1> <label1> <val2> <label2> ...\\n");
							continue;
						}
						chart_index = i - 1 + chart_count - stack_count;
					}
					else {
						if( (dat = strtok(NULL, "")) == NULL) {
							myprintf("ytics usage: #ytics <val1> <label1> <val2> <label2> ...\n");
							continue;
						}
						chart_index = chart_count-1;
					}
					double values[20];
					int i;
					char *labels[20];
					for(i=0; i<20; i++) {
						labels[i] = malloc(150);
					}
					for(i=0; i<20; i++) {
						int nr;
						if(sscanf(dat, "%lf %s%n", &(values[i]), labels[i], &nr) != 2) {
							break;
						}
						myprintf("%g, %s\n", values[i], labels[i]);
						dat = dat + nr;
					}
					if(i >= 2) {
						jbplot_set_y_axis_tics((jbplot *)charts[chart_index].plot,i,values,labels);
						myprintf("setting y tics listed above...\n");
					}
					for(i=0; i<20; i++) {
						free(labels[i]);
					}

				}
				else if(!strcmp(cmd,"xlabel")) {
					if(stack) {
						if(charts[chart_count-1].num_points < 1) {
							myprintf("xlabel must be run after at least one data record (when in stacked mode)\n");
							continue;
						}
						char *c;
						int i;
						if( (c = strtok(NULL, " \t")) == NULL || sscanf(c, "%d", &i) != 1 || 
								i < 1 || i > stack_count ||	(c = strtok(NULL, "")) == NULL ) {
							myprintf("xlabel usage: #xlabel <index (1-based)> <label_text>\n");
							continue;
						}
						jbplot_set_x_axis_label((jbplot *)charts[i - 1 + chart_count - stack_count].plot, c, 1);
						myprintf("Setting x-axis label for plot %d to '%s'\n", i, c);
					}
					else {
						char *c = strtok(NULL, "");
						if(c) {
							jbplot_set_x_axis_label((jbplot *)charts[chart_count-1].plot, c, 1);
							jbplot_set_x_axis_label_visible((jbplot *)charts[chart_count-1].plot, 1);
							myprintf("Setting x-axis label to '%s'\n", c);
						}
						else {
							jbplot_set_x_axis_label_visible((jbplot *)charts[chart_count-1].plot, 0);
							myprintf("Clearing x-axis label\n");
						}
					}
				}
				else if(!strcmp(cmd,"ylabel")) {
					if(stack) {
						if(charts[chart_count-1].num_points < 1) {
							myprintf("ylabel must be run after at least one data record (when in stacked mode)\n");
							continue;
						}
						char *c;
						int i;
						if( (c = strtok(NULL, " \t")) == NULL || sscanf(c, "%d", &i) != 1 || 
								i < 1 || i > stack_count ||	(c = strtok(NULL, "")) == NULL ) {
							myprintf("ylabel usage: #ylabel <index (1-based)> <label_text>\n");
							continue;
						}
						jbplot_set_y_axis_label((jbplot *)charts[i - 1 + chart_count - stack_count].plot, c, 1);
						myprintf("Setting y-axis label for plot %d to '%s'\n", i, c);
					}
					else {
						char *c = strtok(NULL, "");
						if(c) {
							jbplot_set_y_axis_label((jbplot *)charts[chart_count-1].plot, c, 1);
							jbplot_set_y_axis_label_visible((jbplot *)charts[chart_count-1].plot, 1);
							myprintf("Setting y-axis label to '%s'\n", c);
						}
						else {
							jbplot_set_y_axis_label_visible((jbplot *)charts[chart_count-1].plot, 0);
							myprintf("Clearing y-axis label\n");
						}	
					}
				}
				else if(!strcmp(cmd,"title")) {
					if(stack) {
						if(charts[chart_count-1].num_points < 1) {
							myprintf("title must be run after at least one data record (when in stacked mode)\n");
							continue;
						}
						char *c;
						int i;
						if( (c = strtok(NULL, " \t")) == NULL || sscanf(c, "%d", &i) != 1 || 
								i < 1 || i > stack_count ||	(c = strtok(NULL, "")) == NULL ) {
							myprintf("title usage: #title <index (1-based)> <label_text>\n");
							continue;
						}
						jbplot_set_plot_title((jbplot *)charts[i - 1 + chart_count - stack_count].plot, c, 1);
						myprintf("Setting title for plot %d to '%s'\n", i, c);
					}
					else {
						char *c = strtok(NULL, "");
						if(c != NULL && strlen(c) > 0) {
							jbplot_set_plot_title((jbplot *)charts[chart_count-1].plot, c, 1);
							myprintf("Setting title to '%s'\n", c);
						}
					}
				}
				else if(!strcmp(cmd,"xformat")) {
					if(stack) {
						if(charts[chart_count-1].num_points < 1) {
							myprintf("xformat must be run after at least one data record (when in stacked mode)\n");
							continue;
						}
						char *c;
						int i;
						if( (c = strtok(NULL, " \t")) == NULL || sscanf(c, "%d", &i) != 1 || 
								i < 1 || i > stack_count ||	(c = strtok(NULL, "")) == NULL ) {
							myprintf("title usage: #xformat <index (1-based)> <label_text>\n");
							continue;
						}
						jbplot_set_x_axis_format((jbplot *)charts[i - 1 + chart_count - stack_count].plot, c);
						myprintf("Setting x-axis format string for plot %d to '%s'\n", i, c);
					}
					else {
						char *c = strtok(NULL, " \t");
						if(c != NULL) {
							//myprintf("setting x format string to '%s'\n", c);
							jbplot_set_x_axis_format((jbplot *)charts[chart_count-1].plot, c);
							myprintf("Setting x-axis format string to '%s'\n", c);
							got_one = 1;
						}
					}

				}
				else if(!strcmp(cmd,"yformat")) {
					if(stack) {
						if(charts[chart_count-1].num_points < 1) {
							myprintf("yformat must be run after at least one data record (when in stacked mode)\n");
							continue;
						}
						char *c;
						int i;
						if( (c = strtok(NULL, " \t")) == NULL || sscanf(c, "%d", &i) != 1 || 
								i < 1 || i > stack_count ||	(c = strtok(NULL, "")) == NULL ) {
							myprintf("title usage: #yformat <index (1-based)> <label_text>\n");
							continue;
						}
						jbplot_set_y_axis_format((jbplot *)charts[i - 1 + chart_count - stack_count].plot, c);
						myprintf("Setting y-axis format string for plot %d to '%s'\n", i, c);
					}
					else {
						char *c = strtok(NULL, " \t");
						if(c != NULL) {
							//myprintf("setting y format string to '%s'\n", c);
							jbplot_set_y_axis_format((jbplot *)charts[chart_count-1].plot, c);
							myprintf("Setting y-axis format string to '%s'\n", c);
							got_one = 1;
						}
					}
				}
				else {
					myprintf("Invalid command: %s\n", cmd);
				}
			}
			else {
				// first data record for this chart?
				if(charts[chart_count-1].num_points == 0) {
					// count up the number of columns in this row (i.e. # of traces)
					double d;
					int n;
					num_fields = 0;
					char *p = line;
					while(sscanf(p, "%lf%n", &d, &n) == 1) {
						p += n;
						num_fields++;
						if(num_fields > MAX_TRACES) {
							break;
						}
					}
					if(num_fields < 2) {
						myprintf("Incomplete data row.  Skipping...\n");
						continue;
					}

					if(stack) {
						int i;
						myprintf("Got %d fields (%d plots)\n", num_fields, num_fields-1);
						stack_count = num_fields - 1;

						// add more plots if needed
						if(stack_count > 1) {
							for(i=0; i<stack_count-1; i++) {
								add_plot();
							}
						}
					
						// add 1 trace per plot
						for(i=chart_count-stack_count; i<chart_count; i++) {
							add_trace(&(charts[i]));
						}
					}
					else { // no stack
						myprintf("Got %d fields (%d traces)\n", num_fields, num_fields-1);

						// create the traces
						int i;
						for(i=0; i<num_fields-1; i++) {
							add_trace(&(charts[chart_count-1]));
						}
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
						myprintf("error parsing data on line %d\n", line_count);
					}
				}
				else {
					if(valid_fields == num_fields) {
						got_one = 1;
						charts[chart_count-1].num_points++;
						int i;
						for(i=1; i<valid_fields; i++) {
							int j = i - 1 + charts[chart_count-1].num_traces - (num_fields-1);
							//myprintf("adding point: (%lg, %lg)\n", data[0], data[i]); 
							jbplot_trace_add_point(charts[chart_count-1].traces[j], data[0], data[i]); 
						}
					}
					else {
						myprintf("error parsing data on line %d\n", line_count);
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
			int i;
			for(i=0; i<chart_count; i++) {
				jbplot_refresh((jbplot *)charts[i].plot);
			}
		}
		needs_lineup = 1;
	}

	// lineup plot margins if needed
	if(needs_lineup) {
		myprintf("lineup()\n");
		if(lineup_margins() == 0) {
			needs_lineup = 0;
		}
	}

	// if using cmd_line_png option, and STDIN is closed, then create the PNG now
	// and then exit.
	if(cmd_line_png && ret==0) {
		save_png(cmd_line_png_fname);
		exit(0);
	}

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
OPTIONS\n\
------------\n\
 -q \n\
   Quiet mode. Inhibit printing status stuff to STDOUT.\n\
\n\
 -h \n\
   Display this help and exit. \n\
\n\
 -p <fname> \n\
   Save to png and immediately exit. \n\
\n\
INPUT FORMAT\n\
------------\n\
Data is entered using a simple whitespace-delimited text format.  The simplest example \n\
of a single (x,y) data set would look like this, with one (x,y) point per line: \n\
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
Commands are denoted by a '#' character at the beginning of a line.  Command lines may be \n\
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
  NEWTRACE\n\
    Usage: #newtrace \n\
\n\
    Create a new trace (or set of traces).  This command can only be used with stacked mode OFF. \n\
    The data row following this command will be interpretted as having data for a new trace (or \n\
    a number of new traces if the data row has more than 2 columns). \n\
\n\
  STACK\n\
    Usage: #stack <stack_mode=(yes|no|1|0|on|off)> \n\
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
    Set a plot's y-axis label.  Usage similar to XLABEL. \n\
\n\
  XTICS YTICS\n\
    Usage (stacked mode ON): not yet supported \n\
    Usage (stacked mode OFF): #xtics <val1> <label1> <val2> <lab2> ... \n\
\n\
    Allows user-specified tic values-label pairs.\n\
\n\
  XMARGINS\n\
    Usage (stacked mode ON): not yet supported \n\
    Usage (stacked mode OFF): #xmargins <left_px> <right_px> \n\
\n\
  TRACENAME\n\
    Usage: #tracename <trace_index> <name> \n\
\n\
    Sets the legend string for a trace.  The 'trace_index' argument is 1-based.\n\
\n\
  TRACEPROPS\n\
    Usage: #traceprops <trace_index> <line_type> <line_width> <marker_size> <color>\n\
\n\
    Sets various display properties for a trace.  The 'trace_index' argument is 1-based.\n\
    line_type: 0=SOLID; 1=DASHED; 2=DOTTED\n\
    line_width: width of line in pixels (can be fractional)\n\
    marker_size: marker size in pixels (can be fractional)\n\
    color: 0=BLACK; 1=RED; 2=GREEN; 3=BLUE; 4=PINK\n\
\n\
  TITLE\n\
    Set a plot's title text.  Usage similar to XLABEL. \n\
	\n");
	return;
}



int main (int argc, char **argv) {
	int i;
	GtkWidget *top_v_box;
	GtkWidget *h_box;
	GtkWidget *save_button;
	int realtime = 0;

	for(i=1; i<argc; i++) {
		if(!strcmp(argv[i],"-h")) {
			usage(argv[0]);
			return 0;
		}
		else if(!strcmp(argv[i],"-r")) {
			realtime = 1;
		}
		else if(!strcmp(argv[i],"-p")) {
			if((argc-i) >= 2 && argv[i+1][0] != '-') {
				cmd_line_png_fname = argv[++i];
				cmd_line_png = 1;
			}
			else {
				usage(argv[0]);
				return 0;
			}
		}
		else if(!strcmp(argv[i],"-q")) {
			quiet = 1;
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
	gtk_box_pack_start (GTK_BOX(h_box), save_button, FALSE, FALSE, 0);
	g_signal_connect(save_button, "clicked", G_CALLBACK(save_png_cb), NULL);

	cursor_button = gtk_toggle_button_new_with_label("Cursor");
	gtk_box_pack_start (GTK_BOX(h_box), cursor_button, FALSE, FALSE, 0);
	g_signal_connect(cursor_button, "toggled", G_CALLBACK(cursor_cb), NULL);
	//gtk_widget_set_tooltip_text(cursor_button, "Show crosshair when cursor is in plot area");

	coords_button = gtk_toggle_button_new_with_label("Coordinates");
	gtk_box_pack_start (GTK_BOX(h_box), coords_button, FALSE, FALSE, 0);
	g_signal_connect(coords_button, "toggled", G_CALLBACK(coords_cb), NULL);
	//gtk_widget_set_tooltip_text(coords_button, "Show (x,y) coordinates when cursor is in plot area");

	snap_button = gtk_toggle_button_new_with_label("Snap to Data");
	gtk_box_pack_start (GTK_BOX(h_box), snap_button, FALSE, FALSE, 0);
	g_signal_connect(snap_button, "toggled", G_CALLBACK(snap_cb), NULL);
	//gtk_widget_set_tooltip_text(snap_button, "Snap crosshair and/or coordinates to nearest data point");

	if(realtime) {
		clear_all_button = gtk_button_new_with_label("Clear All");
		gtk_box_pack_start (GTK_BOX(h_box), clear_all_button, FALSE, FALSE, 0);
		g_signal_connect(clear_all_button, "clicked", G_CALLBACK(clear_cb), NULL);
	}

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

	g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

	g_timeout_add(50, update_data, NULL);

	if(!cmd_line_png) {
		gtk_widget_show_all(window);
	}

	gtk_main ();

	return 0;
}

