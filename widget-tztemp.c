#include <sys/types.h>
#include <sys/sysctl.h>

#include "settings.h"
#include "builtin-widgets.h"

static int create_widget_private(struct widget *w,
    struct config_format_entry *e, struct config_format_tree *tree);
static void destroy_widget_private(struct widget *w);
static void draw(struct widget *w);
static void clock_tick(struct widget *w);
static int get_temperature(const char *sysctl_oid);

struct widget_interface tztemp_interface = {
	.theme_name 		= "tztemp",
	.size_type 		= WIDGET_SIZE_CONSTANT,
	.create_widget_private 	= create_widget_private,
	.destroy_widget_private = destroy_widget_private,
	.draw 			= draw,
	.clock_tick 		= clock_tick,
};

/* current temperature */
int curtemp;

/**************************************************************************
  TZ Temp "theme" (widget, really)
**************************************************************************/

static int parse_tztemp_theme(struct tztemp_widget *tw,
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
  TZ Temp interface
**************************************************************************/

static int create_widget_private(struct widget *w,
    struct config_format_entry *e, struct config_format_tree *tree)
{
	struct tztemp_widget *tw = xmallocz(sizeof(struct tztemp_widget));
	if (parse_tztemp_theme(tw, e, tree)) {
		xfree(tw);
		XWARNING("Failed to parse tztemp theme");
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
	return 0;
}

static void destroy_widget_private(struct widget *w)
{
	struct tztemp_widget *tw = (struct tztemp_widget *)w->private;

	free_triple_image(&tw->background);
	free_text_info(&tw->font);
	xfree(tw->sysctl_oid);
	xfree(tw);
}

static void draw(struct widget *w)
{
	struct tztemp_widget *tw = (struct tztemp_widget *)w->private;
	char buftemp[8];
	int temp;
	float tempfraq;

	temp = get_temperature(tw->sysctl_oid);
	snprintf(buftemp, sizeof(buftemp), "%d°", temp);

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
	 * 0%R, 60%G, 100%B (HSV: 200, 100, 100) to reddish 100%R, 0%G, 0%B
	 * (HSV: 0, 100, 100)
	 */
	tempfraq = (temp - 30) / 70.0;
	tw->font.color[0] = 255 * tempfraq;
	tw->font.color[1] = 153 * (1 - tempfraq);
	tw->font.color[2] = 255 *  (1 - tempfraq);

	/* text */
	draw_text(cr, w->panel->layout, &tw->font, buftemp, x, 0,
	    centerw, w->panel->height, 0);
}

static void clock_tick(struct widget *w)
{
	struct tztemp_widget *tw = (struct tztemp_widget *)w->private;
	int temp;

	if ((temp = get_temperature(tw->sysctl_oid)) < 0)
		return;

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
