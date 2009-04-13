#include <time.h>
#include <string.h>
#include "builtin-widgets.h"

static int create_widget_private(struct widget *w, struct theme_format_entry *e, 
		struct theme_format_tree *tree);
static void destroy_widget_private(struct widget *w);
static void draw(struct widget *w);
static void clock_tick(struct widget *w);
static void button_click(struct widget *w, XButtonEvent *e);

static int dnd_drop(struct drag_info *di);

static struct widget_interface clock_interface = {
	"clock",
	WIDGET_SIZE_CONSTANT,
	create_widget_private,
	destroy_widget_private,
	draw,
	button_click, /* XXX: tmp */
	clock_tick, /* clock_tick */
	0, /* prop_change */
	0, /* mouse_enter */
	0, /* mouse_leave */
	0, /* mouse_motion */
	0,
	0,
	dnd_drop
};

void register_clock()
{
	register_widget_interface(&clock_interface);
}

/**************************************************************************
  Clock theme
**************************************************************************/

static int parse_clock_theme(struct clock_theme *ct, 
		struct theme_format_entry *e, struct theme_format_tree *tree)
{
	if (parse_triple_image_named(&ct->background, "background", e, tree))
		return xerror("Can't parse 'background' clock triple");

	if (parse_text_info(&ct->font, "font", e)) {
		free_triple_image(&ct->background);
		return xerror("Can't parse 'font' clock entry");
	}

	ct->text_spacing = parse_int("text_spacing", e, 0);
	ct->spacing = parse_int("spacing", e, 0);
	ct->time_format = parse_string("time_format", e, "%H:%M:%S");

	return 0;
}

static void free_clock_theme(struct clock_theme *ct)
{
	free_triple_image(&ct->background);
	free_text_info(&ct->font);
	xfree(ct->time_format);
}

/**************************************************************************
  Clock interface
**************************************************************************/

static int create_widget_private(struct widget *w, struct theme_format_entry *e, 
		struct theme_format_tree *tree)
{
	struct clock_widget *cw = xmallocz(sizeof(struct clock_widget));
	if (parse_clock_theme(&cw->theme, e, tree)) {
		xfree(cw);
		return -1;
	}

	/* get widget width */
	int text_width = 0;
	int pics_width = 0;

	char buftime[128];
	struct tm tm;
	memset(&tm, 0, sizeof(struct tm));
	strftime(buftime, sizeof(buftime), cw->theme.time_format, &tm);

	text_extents(w->panel->layout, cw->theme.font.pfd, 
			buftime, &text_width, 0);

	if (cw->theme.background.left)
		pics_width += cairo_image_surface_get_width(cw->theme.background.left);
	if (cw->theme.background.right)
		pics_width += cairo_image_surface_get_width(cw->theme.background.right);
	w->width = text_width + pics_width + cw->theme.text_spacing +
		cw->theme.spacing * 2;
	w->private = cw;
	return 0;
}

static void destroy_widget_private(struct widget *w)
{
	struct clock_widget *cw = (struct clock_widget*)w->private;
	free_clock_theme(&cw->theme);
	xfree(cw);
}

static void draw(struct widget *w)
{
	struct clock_widget *cw = (struct clock_widget*)w->private;

	/* time */
	char buftime[128];
	time_t current_time;
	current_time = time(0);
	strftime(buftime, sizeof(buftime), cw->theme.time_format, 
			localtime(&current_time));

	/* drawing */
	cairo_t *cr = w->panel->cr;
	int x = w->x + cw->theme.spacing;

	/* calcs */
	int leftw = 0;
	int rightw = 0;
	if (cw->theme.background.left)
		leftw += cairo_image_surface_get_width(cw->theme.background.left);
	if (cw->theme.background.right)
		rightw += cairo_image_surface_get_width(cw->theme.background.right);
	int centerw = w->width - cw->theme.spacing * 2 - leftw - rightw;

	/* left part */
	if (cw->theme.background.left)
		blit_image(cw->theme.background.left, cr, x, w->y);
	x += leftw;

	/* center part */
	pattern_image(cw->theme.background.center, cr, x, w->y, 
			centerw);
	x += centerw;

	/* right part */
	if (cw->theme.background.right)
		blit_image(cw->theme.background.right, cr, x, w->y);

	/* text */
	x -= centerw;
	draw_text(cr, w->panel->layout, &cw->theme.font, buftime, 
			x, w->y, centerw, w->height);
}

static void clock_tick(struct widget *w)
{
	struct clock_widget *cw = (struct clock_widget*)w->private;

	static char buflasttime[128];
	char buftime[128];
	
	time_t current_time;
	current_time = time(0);
	strftime(buftime, sizeof(buftime), cw->theme.time_format, localtime(&current_time));
	if (!strcmp(buflasttime, buftime))
		return;
	strcpy(buflasttime, buftime);

	w->needs_expose = 1;
}

static void button_click(struct widget *w, XButtonEvent *e)
{
	struct clock_widget *cw = (struct clock_widget*)w->private;

	if (cw->theme.text_spacing < 30) {
		cw->theme.text_spacing += 30;
		/* get widget width */
		int text_width = 0;
		int pics_width = 0;
		
		char buftime[128];
		struct tm tm;
		memset(&tm, 0, sizeof(struct tm));
		strftime(buftime, sizeof(buftime), cw->theme.time_format, &tm);

		text_extents(w->panel->layout, cw->theme.font.pfd, 
				buftime, &text_width, 0);

		if (cw->theme.background.left)
			pics_width += cairo_image_surface_get_width(cw->theme.background.left);
		if (cw->theme.background.right)
			pics_width += cairo_image_surface_get_width(cw->theme.background.right);
		w->width = text_width + pics_width + cw->theme.text_spacing +
			cw->theme.spacing * 2;
		recalculate_widgets_sizes(w->panel);
	} else {
		g_main_quit(w->panel->loop);
	}
}

static int dnd_drop(struct drag_info *di)
{
	if (di->dropped_on == 0)
		return 0;
	printf("clock: something dropped here: %s -> %s (ignoring)\n",
			di->taken_on->interface->theme_name,
			di->dropped_on->interface->theme_name);
	return -1;
}
