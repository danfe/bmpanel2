#include <sys/types.h>
#include <sys/sysctl.h>
#include <math.h>

#include "settings.h"
#include "builtin-widgets.h"

static int create_widget_private(struct widget *w,
    struct config_format_entry *e, struct config_format_tree *tree);
static void destroy_widget_private(struct widget *w);
static void draw(struct widget *w);
static void clock_tick(struct widget *w);
static int get_temperature(const char *sysctl_oid);
static void hsv2rgb(float h, float s, float v, float *r, float *g, float *b);

struct widget_interface temperature_interface = {
	.theme_name 		= "temperature",
	.size_type 		= WIDGET_SIZE_CONSTANT,
	.create_widget_private 	= create_widget_private,
	.destroy_widget_private = destroy_widget_private,
	.draw 			= draw,
	.clock_tick 		= clock_tick,
};

/* current temperature */
int curtemp;

/**************************************************************************
  Temperature "theme" (widget, really: no separate theme structure is used)
**************************************************************************/

static int parse_temperature_theme(struct temperature_widget *tw,
    struct config_format_entry *e, struct config_format_tree *tree)
{
	if (parse_text_info_named(&tw->font, "font", e, 1))
		return -1;

	parse_triple_image_named(&tw->background, "background", e, tree, 0);
	tw->sysctl_oid = parse_string("sysctl_oid", e,
	    "hw.acpi.thermal.tz0.temperature");

	return 0;
}

/**************************************************************************
  Temperature interface
**************************************************************************/

static int create_widget_private(struct widget *w,
    struct config_format_entry *e, struct config_format_tree *tree)
{
	struct temperature_widget *tw = xmallocz(sizeof(*tw));

	if (parse_temperature_theme(tw, e, tree)) {
		xfree(tw);
		XWARNING("Failed to parse temperature theme");
		return -1;
	}

	/* get widget width */
	int text_width = 0;
	int pics_width = 0;

	/* this should give us enough width for any real temperature */
	char buftemp[8] = "999°";

	text_extents(w->panel->layout, tw->font.pfd, buftemp, &text_width, 0);

	/* background is drawn only if the center is here */
	if (tw->background.center) {
		pics_width += image_width(tw->background.left);
		pics_width += image_width(tw->background.right);
	}

	w->width = text_width + pics_width;
	w->private = tw;
	curtemp = get_temperature(tw->sysctl_oid);
	return 0;
}

static void destroy_widget_private(struct widget *w)
{
	struct temperature_widget *tw = w->private;

	free_triple_image(&tw->background);
	free_text_info(&tw->font);
	xfree(tw->sysctl_oid);
	xfree(tw);
}

static void draw(struct widget *w)
{
	struct temperature_widget *tw = w->private;
	char buftemp[8];
	static int blink;
	float r, g, b;

	if (blink && curtemp > 95)
		*buftemp = '\0';
	else
		snprintf(buftemp, sizeof(buftemp), "%d°", curtemp);
	blink = curtemp > 95 ? !blink : 0;

	/* drawing */
	cairo_t *cr = w->panel->cr;
	int x = w->x;

	/* calcs */
	int leftw = 0;
	int rightw = 0;
	int centerw = w->width;

	/* draw background only if the center image is here */
	if (tw->background.center) {
		leftw += image_width(tw->background.left);
		rightw += image_width(tw->background.right);
		centerw -= leftw + rightw;

		/* left part */
		if (leftw)
			blit_image(tw->background.left, cr, x, 0);
		x += leftw;

		/* center part */
		pattern_image(tw->background.center, cr, x, 0, centerw, 1);
		x += centerw;

		/* right part */
		if (rightw)
			blit_image(tw->background.right, cr, x, 0);
		x -= centerw;
	}

	/*
	 * map temperature (30C~100C) to the text color: from nice blueish
	 * 0%R, 60%G, 100%B (HSV: 200, 100%, 100%) to reddish 100%R, 0%G,
	 * 0%B (HSV: 0, 100%, 100%) through the hue shift (think rainbow).
	 */
	hsv2rgb((200 / 360.0) * (1 - (curtemp - 30) / 70.0), 1, 1, &r, &g, &b);
	tw->font.color[0] = 255 * r;
	tw->font.color[1] = 255 * g;
	tw->font.color[2] = 255 * b;

	/* text */
	draw_text(cr, w->panel->layout, &tw->font, buftemp, x, 0,
	    centerw, w->panel->height, 0);
}

static void clock_tick(struct widget *w)
{
	struct temperature_widget *tw = w->private;
	int temp;

	if ((temp = get_temperature(tw->sysctl_oid)) < 0)
		return;

	if (temp > 95)
		w->needs_expose = 1;

	if (curtemp == temp)
		return;

	curtemp = temp;
	w->needs_expose = 1;
}

static int get_temperature(const char *sysctl_oid)
{
	int temp;
	size_t len = sizeof(temp);

	if (sysctlbyname(sysctl_oid, &temp, &len, NULL, 0))
		return -1;
	return (temp - 2732) / 10;
}

static void hsv2rgb(float h, float s, float v, float *r, float *g, float *b)
{
	float f, p, q, t;
	int i;

	/* achromatic case, set level of gray */
	if (s <= 0) {
		*r = *g = *b = v;
		return;
	}

	h = 6.0 * (h - floorf(h));

	i = (int)truncf(h);		/* should be in the range 0..5 */
	f = h - i;			/* fractional part */
	p = v * (1 - s);
	q = v * (1 - s * f);
	t = v * (1 - s * (1 - f));
	
	switch (i) {
	case 0:
		*r = v; *g = t; *b = p;
		return;
	case 1:
		*r = q; *g = v; *b = p;
		return;
	case 2:
		*r = p; *g = v; *b = t;
		return;
	case 3:
		*r = p; *g = q; *b = v;
		return;
	case 4:
		*r = t; *g = p; *b = v;
		return;
	case 5:
	default:			/* to silence compiler warning */
		*r = v; *g = p; *b = q;
		return;
	}
}
