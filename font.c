/* #define FONT_TEST */
#ifdef FONT_TEST
#endif

#ifdef FONT_TEST

void SetFontRecursively2(GtkWidget *widget, gpointer data)
{
   GtkStyle *style;
   char *font_desc;

   font_desc = (char *)data;

   g_print("font desc=[%s]\n", font_desc);
   style = gtk_widget_get_style(widget);
   pango_font_description_free(style->font_desc);
   style->font_desc = pango_font_description_from_string(font_desc);
   gtk_widget_set_style(widget, style);
   if (GTK_IS_CONTAINER(widget)) {
      gtk_container_foreach(GTK_CONTAINER(widget), SetFontRecursively2, font_desc);
   }
}
void font_selection_ok(GtkWidget *w, GtkFontSelectionDialog *fs)
{
   gchar *s = gtk_font_selection_dialog_get_font_name(fs);


   SetFontRecursively2(window, s);

   g_free(s);
   gtk_widget_destroy(GTK_WIDGET(fs));
   cb_app_button(NULL, GINT_TO_POINTER(REDRAW));
}

void font_sel_dialog()
{
   static GtkWidget *fontsel = NULL;

   if (!fontsel) {
      fontsel = gtk_font_selection_dialog_new(_("Font Selection Dialog"));
      gtk_window_set_position(GTK_WINDOW(fontsel), GTK_WIN_POS_MOUSE);

      gtk_signal_connect(GTK_OBJECT(fontsel), "destroy",
			 GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			 &fontsel);

      gtk_window_set_modal(GTK_WINDOW(fontsel), TRUE);

      gtk_signal_connect(GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(fontsel)->ok_button),
			 "clicked", GTK_SIGNAL_FUNC(font_selection_ok),
			 GTK_FONT_SELECTION_DIALOG(fontsel));
      gtk_signal_connect_object(GTK_OBJECT(GTK_FONT_SELECTION_DIALOG(fontsel)->cancel_button),
				"clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy),
				GTK_OBJECT(fontsel));
     }

   if (!GTK_WIDGET_VISIBLE(fontsel)) {
      gtk_widget_show(fontsel);
   } else {
      gtk_widget_destroy(fontsel);
   }
}

/*fixme move to font.c */
void cb_font(GtkWidget *widget, gpointer data)
{
   font_sel_dialog();

   return;
}
#endif

#ifdef FONT_TEST
/*   f=gdk_fontset_load("-adobe-utopia-medium-r-normal-*-*-200-*-*-p-*-iso8859-1");
   f=gdk_fontset_load("-adobe-utopia-bold-i-normal-*-*-100-*-*-p-*-iso8859-2");
   SetFontRecursively(window, f);
   SetFontRecursively2(window, "Sans 14");
   SetFontRecursively2(menubar, "Sans 16");*/
#endif

#ifdef FONT_TEST
   /* Create "Font" button */
   button = gtk_button_new_with_label(_("Font"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_font), NULL);

   gtk_box_pack_start(GTK_BOX(g_vbox0), button, FALSE, FALSE, 0);
#endif
