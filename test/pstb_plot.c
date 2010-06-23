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

static int t = 0;
static trace_handle t1;
GtkWidget *plot;
static int run = 1;
#define LINE_SIZE 1000
static char line[LINE_SIZE];
static char line_index = 0;
static int line_count = 0;

char *strip_leading_ws(char *s) {
	int i=0;
	while(isspace(s[i])) {
		i++;
	}
	return s+i;
}

gboolean update_data(gpointer data) {
	int i;
	int ret;
	double x, y;
	char c;
	int got_one = 0;
	int is_cmd = 0;
	if(!run) {
		return TRUE;
	}

	while((ret = read(0, &c, 1)) > 0) {
		//printf("got char: %c\n", c);
		if(c == '\n') {
			line[line_index] = '\0';
			line_count++;
			line_index = 0;
			//printf("got line: %s\n", line);
			if(is_cmd) {
				char *cmd;
				cmd = strtok(strip_leading_ws(line), " \t"); 
				printf("Got command: %s\n", cmd);
				if(!strcmp(cmd,"xlabel")) {
					//printf("xlabel handler\n");
					jbplot_set_x_axis_label((jbplot *)plot, cmd+7, 1);
				}
				else if(!strcmp(cmd,"ylabel")) {
					//printf("ylabel handler\n");
					jbplot_set_y_axis_label((jbplot *)plot, cmd+7, 1);
				}
				else if(!strcmp(cmd,"title")) {
					//printf("title handler\n");
					jbplot_set_plot_title((jbplot *)plot, cmd+6, 1);
				}
				
			}
			else {
				if(sscanf(line, "%lf %lf", &x, &y) != 2) {
					printf("error parsing x y on line %d\n", line_count);
				}
				else {
					got_one = 1;
					jbplot_trace_add_point(t1, x, y); 
				}
			}
		}
		else if(line_index==0 && c == '#') {
			is_cmd = 1;
		}
		else {
			line[line_index++] = c;
		}
	}		

	if(got_one) {
		jbplot_refresh((jbplot *)plot);
	}
	return TRUE;
}

void button_activate(GtkButton *b, gpointer data) {
	run = !run;
	if(run) {
		gtk_button_set_label(b, "Pause");
	}
	else {
		gtk_button_set_label(b, "Resume");
	}
	return;
}

int main (int argc, char **argv) {
	GtkWidget *window;
	GtkWidget *v_box;
	GtkWidget *button;

	gtk_init (&argc, &argv);

	int flags;
	flags = fcntl(0, F_GETFL, 0);
  flags |= O_NONBLOCK;
  fcntl(0, F_SETFL, flags);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	v_box = gtk_vbox_new(FALSE, 10);
	gtk_container_add (GTK_CONTAINER (window), v_box);
	
	plot = jbplot_new ();
	gtk_widget_set_size_request(plot, 700, 700);
	gtk_box_pack_start (GTK_BOX(v_box), plot, TRUE, TRUE, 0);

	button = gtk_button_new_with_label("Pause");
	gtk_box_pack_start (GTK_BOX(v_box), button, FALSE, FALSE, 0);
	g_signal_connect(button, "clicked", G_CALLBACK(button_activate), NULL);

	g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

	gtk_widget_show_all (window);

	g_timeout_add(50, update_data, t1);

	jbplot_set_plot_title((jbplot *)plot, "PSTB plot", 1);
	jbplot_set_plot_title_visible((jbplot *)plot, 1);
	jbplot_set_x_axis_label((jbplot *)plot, "Time (sec)", 1);
	jbplot_set_x_axis_label_visible((jbplot *)plot, 1);
	jbplot_set_y_axis_label((jbplot *)plot, "Amplitude", 1);
	jbplot_set_y_axis_label_visible((jbplot *)plot, 1);


	jbplot_set_legend_props((jbplot *)plot, 1.0, NULL, NULL, LEGEND_POS_RIGHT);
	//jbplot_set_legend_props((jbplot *)plot, 1.0, NULL, NULL, LEGEND_POS_TOP);
	jbplot_legend_refresh((jbplot *)plot);

	rgb_color_t gridline_color = {0.7, 0.7, 0.7};
	jbplot_set_x_axis_gridline_props((jbplot *)plot, LINETYPE_DASHED, 1.0, &gridline_color);
	jbplot_set_y_axis_gridline_props((jbplot *)plot, LINETYPE_DASHED, 1.0, &gridline_color);

	t1 = jbplot_create_trace(2000);
	if(t1==NULL) {
		printf("error creating trace!\n");
		return 0;
	}
	rgb_color_t color = {0.0, 1.0, 0.0};
	jbplot_trace_set_line_props(t1, LINETYPE_SOLID, 1.0, &color);
	color.red = 1.0; color.green = 0.0;	color.blue = 0.0;
	color.red = 0.0; color.green = 0.0;	color.blue = 1.0;
	jbplot_trace_set_marker_props(t1, MARKER_CIRCLE, 2.0, &color);
	jbplot_add_trace((jbplot *)plot, t1);

	gtk_main ();

	return 0;
}
