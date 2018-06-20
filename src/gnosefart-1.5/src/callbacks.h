/*
    Copyright (C) 2004 Matthew Strait <quadong@users.sf.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#include <gtk/gtk.h>


void
close_gnosefart                        (GtkObject       *object,
                                        gpointer         user_data);

void
track_spinbutton_changed               (GtkEditable     *editable,
                                        gpointer         user_data);

void
forever_toggled                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
seconds_toggled                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
repetitions_toggled                    (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
seconds_spinbutton_changed             (GtkEditable     *editable,
                                        gpointer         user_data);

void
repetitions_spinbutton_changed         (GtkEditable     *editable,
                                        gpointer         user_data);

void
channel1_toggled                       (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
channel2_toggled                       (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
channel3_toggled                       (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
channel4_toggled                       (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
channel5_toggled                       (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
channel6_toggled                       (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
play_pushed                            (GtkButton       *button,
                                        gpointer         user_data);

void
stop_pushed                            (GtkButton       *button,
                                        gpointer         user_data);

void
help_pushed                            (GtkButton       *button,
                                        gpointer         user_data);

void
on_ok_button1_released                 (GtkButton       *button,
                                        gpointer         user_data);

void
on_cancel_button1_pressed              (GtkButton       *button,
                                        gpointer         user_data);

void
ok_released_in_help                    (GtkButton       *button,
                                        gpointer         user_data);


void
close_pushed                           (GtkButton       *button,
                                        gpointer         user_data);

void
closebutton_pushed_in_error_window     (GtkButton       *button,
                                        gpointer         user_data);

void
closebutton_pushed_in_nnf_error_window (GtkButton       *button,
                                        gpointer         user_data);



void
statusbar1_show                        (GtkWidget       *widget,
                                        gpointer         user_data);

void
open_pushed                            (GtkButton       *button,
                                        gpointer         user_data);

void
open_pushed2                           (GtkButton       *button,
                                        gpointer         user_data);

void
open_pushed                            (GtkButton       *button,
                                        gpointer         user_data);

void
close_pushed                           (GtkButton       *button,
                                        gpointer         user_data);

void
help_pushed                            (GtkButton       *button,
                                        gpointer         user_data);

void
on_ok_button1_released                 (GtkButton       *button,
                                        gpointer         user_data);

void
on_cancel_button1_pressed              (GtkButton       *button,
                                        gpointer         user_data);

void
ok_released_in_help                    (GtkButton       *button,
                                        gpointer         user_data);

void
close_button_pushed_in_error_window    (GtkButton       *button,
                                        gpointer         user_data);

void
closebutton_pushed_in_nnf_error_window (GtkButton       *button,
                                        gpointer         user_data);

void
closebutton_pushed_in_error_window     (GtkButton       *button,
                                        gpointer         user_data);

void
spinbutton3_map                        (GtkWidget       *widget,
                                        gpointer         user_data);

void
statusbar1_map                         (GtkWidget       *widget,
                                        gpointer         user_data);

void
about_button_pushed                    (GtkButton       *button,
                                        gpointer         user_data);

void
ok_released_in_about                   (GtkButton       *button,
                                        gpointer         user_data);

void
on_fileselection_map                   (GtkWidget       *widget,
                                        gpointer         user_data);

void
spinbutton_mult_changed                (GtkEditable     *editable,
                                        gpointer         user_data);

void
dialog2_mapped                         (GtkWidget       *widget,
                                        gpointer         user_data);

void
on_dialog2_map                         (GtkWidget       *widget,
                                        gpointer         user_data);

gboolean
on_dialog2_destroy_event               (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_dialog2_close                       (GtkDialog       *dialog,
                                        gpointer         user_data);

gboolean
on_dialog2_destroy_event               (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

gboolean
on_dialog2_delete_event                (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_dialog2_destroy                     (GtkObject       *object,
                                        gpointer         user_data);





void
closebutton_pushed_in_audio_error_window
                                        (GtkButton       *button,
                                        gpointer         user_data);

void
on_fileselection_response              (GtkDialog       *dialog,
                                        gint             response_id,
                                        gpointer         user_data);

void
fileselection_sr                       (GtkWidget       *widget,
                                        GtkSelectionData *data,
                                        guint            time,
                                        gpointer         user_data);

gboolean
mainwindow_key_press                   (GtkWidget       *widget,
                                        GdkEventKey     *event,
                                        gpointer         user_data);

void
tonon_togglebut1_map                   (GtkWidget       *widget,
                                        gpointer         user_data);

void
on_togglebutton2_map                   (GtkWidget       *widget,
                                        gpointer         user_data);

void
on_togglebutton3_map                   (GtkWidget       *widget,
                                        gpointer         user_data);

void
on_togglebutton4_map                   (GtkWidget       *widget,
                                        gpointer         user_data);

void
on_togglebutton5_map                   (GtkWidget       *widget,
                                        gpointer         user_data);

void
on_togglebutton6_map                   (GtkWidget       *widget,
                                        gpointer         user_data);

void
on_togglebutton1_map                   (GtkWidget       *widget,
                                        gpointer         user_data);

void
forever_button_map                     (GtkWidget       *widget,
                                        gpointer         user_data);

void
seconds_button_map                     (GtkWidget       *widget,
                                        gpointer         user_data);

void
repetitions_button_map                 (GtkWidget       *widget,
                                        gpointer         user_data);
