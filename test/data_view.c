/**
 * main.c
 */

#include <gtk/gtk.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../jbplot.h"

#define LINE_BUFFER_CHUNK_SIZE 1000



char *get_line(FILE *fp) {
	static char *line = NULL;
	static int line_size = 0;
	int index = 0;
	char c;
	if(line == NULL) {
		line = malloc(LINE_BUFFER_CHUNK_SIZE);
		if(line == NULL) {
			return NULL;
		}
		line_size = LINE_BUFFER_CHUNK_SIZE;
	}
	while(1) {
		c=fgetc(fp);
		if(c == EOF || c == '\n') { // done
			line[index] = '\0';
			break;
		}
		line[index++] = c;
		if(index >= line_size) {
			line = realloc(line, line_size + LINE_BUFFER_CHUNK_SIZE);
			if(line == NULL) {
				return NULL;
			}
			line_size += LINE_BUFFER_CHUNK_SIZE;
		}

	}
	return line;
}


static trace_handle t1, t2;
GtkWidget *plot;
FILE *fp;


void load_data(GtkButton *b, gpointer data) {
	GtkWidget *dialog;
	dialog = gtk_file_chooser_dialog_new("Save file to...",
	                                     (GtkWindow *)plot,
	                                     GTK_FILE_CHOOSER_ACTION_OPEN,
	                                     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	                                     GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
	                                     NULL);
	if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		char *filename;
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		printf("selected file: %s\n", filename);
		fp = fopen(filename, "r");
		if(fp == NULL) {
			printf("Error opening file\n");
		}
		else {
			char *line;
			printf("got here!\n");
			if((line=get_line(fp)) == NULL) {
				printf("Error reading 1st line\n");
			}
			else {
				printf("first line: %s\n", line);
			}
			fclose(fp);
		}
		g_free(filename);
	}
	gtk_widget_destroy(dialog);
	return;
}



int main (int argc, char **argv) {
	GtkWidget *window;
	GtkWidget *v_box;
	GtkWidget *h_box;
	GtkWidget *load_button;
	double start_dt;

	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	h_box = gtk_hbox_new(FALSE, 10);
	gtk_container_add (GTK_CONTAINER (window), h_box);

	v_box = gtk_vbox_new(FALSE, 10);
	gtk_box_pack_start(GTK_BOX (h_box), v_box, FALSE, FALSE, 0);
	
	plot = jbplot_new ();
	gtk_widget_set_size_request(plot, 700, 400);
	jbplot_set_antialias((jbplot *)plot, 0);
	gtk_box_pack_start (GTK_BOX(h_box), plot, TRUE, TRUE, 0);

	load_button = gtk_button_new_with_label("Load Data...");
	gtk_box_pack_start (GTK_BOX(v_box), load_button, FALSE, FALSE, 0);
	g_signal_connect(load_button, "clicked", G_CALLBACK(load_data), NULL);

	g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

	gtk_widget_show_all (window);

	jbplot_set_plot_title((jbplot *)plot, "Double Pendulum", 1);
	jbplot_set_plot_title_visible((jbplot *)plot, 1);
	jbplot_set_x_axis_label((jbplot *)plot, "Time (sec)", 1);
	jbplot_set_x_axis_label_visible((jbplot *)plot, 1);
	jbplot_set_y_axis_label((jbplot *)plot, "Amplitude", 1);
	jbplot_set_y_axis_label_visible((jbplot *)plot, 1);

	rgb_color_t gridline_color = {0.7, 0.7, 0.7};
	jbplot_set_x_axis_gridline_props((jbplot *)plot, LINETYPE_DASHED, 1.0, &gridline_color);
	jbplot_set_y_axis_gridline_props((jbplot *)plot, LINETYPE_DASHED, 1.0, &gridline_color);
	jbplot_set_legend_props((jbplot *)plot, 1, NULL, NULL, LEGEND_POS_RIGHT);

	t1 = jbplot_create_trace(3000);
	t2 = jbplot_create_trace(3000);
	if(t1==NULL || t2==NULL) {
		printf("error creating traces!\n");
		return 0;
	}
	rgb_color_t color = {0.0, 1.0, 0.0};
	jbplot_trace_set_line_props(t1, LINETYPE_SOLID, 2.0, &color);
	color.red = 1.0; color.green = 0.0;	color.blue = 0.0;
	jbplot_trace_set_line_props(t2, LINETYPE_SOLID, 2.0, &color);
	jbplot_trace_set_name(t1, "theta_1");
	jbplot_trace_set_name(t2, "theta_2");
	jbplot_add_trace((jbplot *)plot, t1);
	jbplot_add_trace((jbplot *)plot, t2);

	gtk_main ();

	return 0;
}
