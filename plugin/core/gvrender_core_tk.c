/* $Id$ $Revision$ */
/* vim:set shiftwidth=4 ts=8: */

/**********************************************************
*      This software is part of the graphviz package      *
*                http://www.graphviz.org/                 *
*                                                         *
*            Copyright (c) 1994-2004 AT&T Corp.           *
*                and is licensed under the                *
*            Common Public License, Version 1.0           *
*                      by AT&T Corp.                      *
*                                                         *
*        Information and Software Systems Research        *
*              AT&T Research, Florham Park NJ             *
**********************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"
#include "const.h"

#include "gvplugin_render.h"
#include "gvplugin_device.h"
#include "gvio.h"
#include "gvcint.h"

typedef enum { FORMAT_TK, } format_type;

static char *tkgen_string(char *s)
{
    return s;
}

static void tkgen_print_color(GVJ_t * job, gvcolor_t color)
{
    switch (color.type) {
    case COLOR_STRING:
	gvputs(job, color.u.string);
	break;
    case RGBA_BYTE:
	if (color.u.rgba[3] == 0) /* transparent */
	    gvputs(job, "\"\"");
	else
	    gvprintf(job, "#%02x%02x%02x",
		color.u.rgba[0], color.u.rgba[1], color.u.rgba[2]);
	break;
    default:
	assert(0);		/* internal error */
    }
}

static void tkgen_print_tags(GVJ_t *job)
{
    char *ObjType, *ObjPart;
    unsigned int ObjId;
    obj_state_t *obj = job->obj;
    int ObjFlag;
#ifndef WITH_CGRAPH
    int ObjHandle;
#endif

    switch (obj->emit_state) {
    case EMIT_NDRAW:
	ObjType = "node";
	ObjPart = "shape";
	ObjFlag = 1;
        ObjId = AGID(obj->u.n);
#ifndef WITH_CGRAPH
	ObjHandle = obj->u.n->handle;
#endif
	break;
    case EMIT_NLABEL:
	ObjType = "node";
	ObjPart = "label";
	ObjFlag = 0;
        ObjId = AGID(obj->u.n);
#ifndef WITH_CGRAPH
	ObjHandle = obj->u.n->handle;
#endif
	break;
    case EMIT_EDRAW:
    case EMIT_TDRAW:
    case EMIT_HDRAW:
	ObjType = "edge";
	ObjPart = "shape";
	ObjFlag = 1;
        ObjId = AGID(obj->u.e);
#ifndef WITH_CGRAPH
	ObjHandle = obj->u.e->handle;
#endif
	break;
    case EMIT_ELABEL:
    case EMIT_TLABEL:
    case EMIT_HLABEL:
	ObjType = "edge";
	ObjPart = "label";
	ObjFlag = 0;
        ObjId = AGID(obj->u.e);
#ifndef WITH_CGRAPH
	ObjHandle = obj->u.e->handle;
#endif
	break;
    case EMIT_GDRAW:
	ObjType = "graph";
	ObjPart = "shape";
	ObjFlag = 1;
	ObjId = -1;  /* hack! */
#ifndef WITH_CGRAPH
	ObjHandle = obj->u.g->handle;
#endif
	break;
    case EMIT_GLABEL:
	ObjType = "graph";
	ObjPart = "label";
	ObjFlag = 0;
	ObjType = "graph label";
	ObjId = -1;  /* hack! */
#ifndef WITH_CGRAPH
	ObjHandle = obj->u.g->handle;
#endif
	break;
    case EMIT_CDRAW:
	ObjType = "graph";
	ObjPart = "shape";
	ObjFlag = 1;
#ifndef WITH_CGRAPH
	ObjId = obj->u.sg->meta_node->id;
	ObjHandle = obj->u.sg->handle;
#else
	ObjId = AGID(obj->u.sg);
#endif
	break;
    case EMIT_CLABEL:
	ObjType = "graph";
	ObjPart = "label";
	ObjFlag = 0;
#ifndef WITH_CGRAPH
	ObjId = obj->u.sg->meta_node->id;
	ObjHandle = obj->u.sg->handle;
#else
	ObjId = AGID(obj->u.sg);
#endif
	break;
    default:
	assert (0);
	break;
    }
#ifndef WITH_CGRAPH
    gvprintf(job, " -tags {%d%s%d}", ObjFlag, ObjType, ObjHandle);
#else
    gvprintf(job, " -tags {%d%s%d}", ObjFlag, ObjType, ObjId);
#endif
}

static void tkgen_canvas(GVJ_t * job)
{
   if (job->external_context) 
	gvputs(job, job->imagedata);
   else
	gvputs(job, "$c");
}

static void tkgen_comment(GVJ_t * job, char *str)
{
    gvputs(job, "# ");
    gvputs(job, tkgen_string(str));
    gvputs(job, "\n");
}

static void tkgen_begin_job(GVJ_t * job)
{
    gvputs(job, "# Generated by ");
    gvputs(job, tkgen_string(job->common->info[0]));
    gvputs(job, " version ");
    gvputs(job, tkgen_string(job->common->info[1]));
    gvputs(job, " (");
    gvputs(job, tkgen_string(job->common->info[2]));
    gvputs(job, ")\n");
}

static void tkgen_begin_graph(GVJ_t * job)
{
    obj_state_t *obj = job->obj;

    gvputs(job, "#");
    if (agnameof(obj->u.g)[0]) {
        gvputs(job, " Title: ");
	gvputs(job, tkgen_string(agnameof(obj->u.g)));
    }
    gvprintf(job, " Pages: %d\n", job->pagesArraySize.x * job->pagesArraySize.y);
}

static int first_periphery;

static void tkgen_begin_node(GVJ_t * job)
{
	first_periphery = 1;     /* FIXME - this is an ugly hack! */
}

static void tkgen_begin_edge(GVJ_t * job)
{
	first_periphery = -1;     /* FIXME - this is an ugly ugly hack!  Need this one for arrowheads. */
}

static void tkgen_textpara(GVJ_t * job, pointf p, textpara_t * para)
{
    obj_state_t *obj = job->obj;
    const char *font;
    int size;

    if (obj->pen != PEN_NONE) {
	/* determine font size */
	/* round fontsize down, better too small than too big */
	size = (int)(para->fontsize * job->zoom);
	/* don't even bother if fontsize < 1 point */
	if (size)  {
            tkgen_canvas(job);
            gvputs(job, " create text ");
            p.y -= size * 0.55; /* cl correction */
            gvprintpointf(job, p);
            gvputs(job, " -text {");
            gvputs(job, para->str);
            gvputs(job, "}");
            gvputs(job, " -fill ");
            tkgen_print_color(job, obj->pencolor);
            gvputs(job, " -font {");
	    /* tk doesn't like PostScript font names like "Times-Roman" */
	    /*    so use family names */
	    if (para->postscript_alias)
	        font = para->postscript_alias->family;
	    else
		font = para->fontname;
            gvputs(job, "\"");
            gvputs(job, font);
            gvputs(job, "\"");
	    /* use -ve fontsize to indicate pixels  - see "man n font" */
            gvprintf(job, " %d}", size);
            switch (para->just) {
            case 'l':
                gvputs(job, " -anchor w");
                break;
            case 'r':
                gvputs(job, " -anchor e");
                break;
            default:
            case 'n':
                break;
            }
            tkgen_print_tags(job);
            gvputs(job, "\n");
        }
    }
}

static void tkgen_ellipse(GVJ_t * job, pointf * A, int filled)
{
    obj_state_t *obj = job->obj;
    pointf r;

    if (obj->pen != PEN_NONE) {
    /* A[] contains 2 points: the center and top right corner. */
        r.x = A[1].x - A[0].x;
        r.y = A[1].y - A[0].y;
        A[0].x -= r.x;
        A[0].y -= r.y;
        tkgen_canvas(job);
        gvputs(job, " create oval ");
        gvprintpointflist(job, A, 2);
        gvputs(job, " -fill ");
        if (filled)
            tkgen_print_color(job, obj->fillcolor);
        else if (first_periphery)
	    /* tk ovals default to no fill, some fill
             * is necessary else "canvas find overlapping" doesn't
             * work as expected, use white instead */
	    gvputs(job, "white");
	else 
	    gvputs(job, "\"\"");
	if (first_periphery == 1)
	    first_periphery = 0;
        gvputs(job, " -width ");
        gvprintdouble(job, obj->penwidth);
        gvputs(job, " -outline ");
	tkgen_print_color(job, obj->pencolor);
        if (obj->pen == PEN_DASHED)
	    gvputs(job, " -dash 5");
        if (obj->pen == PEN_DOTTED)
	    gvputs(job, " -dash 2");
        tkgen_print_tags(job);
        gvputs(job, "\n");
    }
}

static void
tkgen_bezier(GVJ_t * job, pointf * A, int n, int arrow_at_start,
	      int arrow_at_end, int filled)
{
    obj_state_t *obj = job->obj;

    if (obj->pen != PEN_NONE) {
        tkgen_canvas(job);
        gvputs(job, " create line ");
        gvprintpointflist(job, A, n);
        gvputs(job, " -fill ");
        tkgen_print_color(job, obj->pencolor);
        gvputs(job, " -width ");
        gvprintdouble(job, obj->penwidth);
        if (obj->pen == PEN_DASHED)
	    gvputs(job, " -dash 5");
        if (obj->pen == PEN_DOTTED)
	    gvputs(job, " -dash 2");
        gvputs(job, " -smooth bezier ");
        tkgen_print_tags(job);
        gvputs(job, "\n");
    }
}

static void tkgen_polygon(GVJ_t * job, pointf * A, int n, int filled)
{
    obj_state_t *obj = job->obj;

    if (obj->pen != PEN_NONE) {
        tkgen_canvas(job);
        gvputs(job, " create polygon ");
        gvprintpointflist(job, A, n);
        gvputs(job, " -fill ");
        if (filled)
            tkgen_print_color(job, obj->fillcolor);
        else if (first_periphery)
            /* tk polygons default to black fill, some fill
	     * is necessary else "canvas find overlapping" doesn't
	     * work as expected, use white instead */
            gvputs(job, "white");
        else
            gvputs(job, "\"\"");
	if (first_periphery == 1) 
	    first_periphery = 0;
        gvputs(job, " -width ");
        gvprintdouble(job, obj->penwidth);
        gvputs(job, " -outline ");
	tkgen_print_color(job, obj->pencolor);
        if (obj->pen == PEN_DASHED)
	    gvputs(job, " -dash 5");
        if (obj->pen == PEN_DOTTED)
	    gvputs(job, " -dash 2");
        tkgen_print_tags(job);
        gvputs(job, "\n");
    }
}

static void tkgen_polyline(GVJ_t * job, pointf * A, int n)
{
    obj_state_t *obj = job->obj;

    if (obj->pen != PEN_NONE) {
        tkgen_canvas(job);
        gvputs(job, " create line ");
        gvprintpointflist(job, A, n);
        gvputs(job, " -fill ");
        tkgen_print_color(job, obj->pencolor);
        if (obj->pen == PEN_DASHED)
	    gvputs(job, " -dash 5");
        if (obj->pen == PEN_DOTTED)
	    gvputs(job, " -dash 2");
        tkgen_print_tags(job);
        gvputs(job, "\n");
    }
}

gvrender_engine_t tkgen_engine = {
    tkgen_begin_job,
    0,				/* tkgen_end_job */
    tkgen_begin_graph,
    0,				/* tkgen_end_graph */
    0, 				/* tkgen_begin_layer */
    0, 				/* tkgen_end_layer */
    0, 				/* tkgen_begin_page */
    0, 				/* tkgen_end_page */
    0, 				/* tkgen_begin_cluster */
    0, 				/* tkgen_end_cluster */
    0,				/* tkgen_begin_nodes */
    0,				/* tkgen_end_nodes */
    0,				/* tkgen_begin_edges */
    0,				/* tkgen_end_edges */
    tkgen_begin_node,
    0,				/* tkgen_end_node */
    tkgen_begin_edge,
    0,				/* tkgen_end_edge */
    0,				/* tkgen_begin_anchor */
    0,				/* tkgen_end_anchor */
    0,				/* tkgen_begin_label */
    0,				/* tkgen_end_label */
    tkgen_textpara,
    0,				/* tkgen_resolve_color */
    tkgen_ellipse,
    tkgen_polygon,
    tkgen_bezier,
    tkgen_polyline,
    tkgen_comment,
    0,				/* tkgen_library_shape */
};

gvrender_features_t render_features_tk = {
    GVRENDER_Y_GOES_DOWN
	| GVRENDER_NO_WHITE_BG, /* flags */
    4.,                         /* default pad - graph units */
    NULL, 			/* knowncolors */
    0,				/* sizeof knowncolors */
    COLOR_STRING,		/* color_type */
};

gvdevice_features_t device_features_tk = {
    0,				/* flags */
    {0.,0.},			/* default margin - points */
    {0.,0.},                    /* default page width, height - points */
    {96.,96.},			/* default dpi */
};

gvplugin_installed_t gvrender_tk_types[] = {
    {FORMAT_TK, "tk", 1, &tkgen_engine, &render_features_tk},
    {0, NULL, 0, NULL, NULL}
};

gvplugin_installed_t gvdevice_tk_types[] = {
    {FORMAT_TK, "tk:tk", 1, NULL, &device_features_tk},
    {0, NULL, 0, NULL, NULL}
};
