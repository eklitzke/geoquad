#include <Python.h>

#include "data.h"
#include <stdio.h>
#include <stdint.h>

#include <stdlib.h>

#define LONGITUDE_MIN  -180.0
#define LONGITUDE_MAX   180.0
#define LATITUDE_MIN    -90.0
#define LATITUDE_MAX     90.0
#define GEOQUAD_STEP     0.05

/* Interleaved ones and zeroes, LSB = 1 */
#define INTER16L 0x5555
#define INTER32L 0x55555555

/* Interleaved ones and zeroes, MSB = 1 */
#define INTER16M 0xAAAA
#define INTER32M 0xAAAAAAAA

struct quad_s
{
	float nw;
	float ne;
	float se;
	float sw;
};

/* A half interleave/ */
static inline uint32_t interleave_half(uint16_t x)
{
	return (morton_forward[x >> 8] << 16) | morton_forward[x & 0xFF];
}

/* A full interleave */
static inline uint32_t interleave_full(uint16_t x, uint16_t y)
{
	return interleave_half(x) | (interleave_half(y) << 1);
}

/* A half deinterleave */
static inline uint16_t deinterleave_half(uint32_t z)
{
	return morton_sparse[z & INTER16L] | (morton_sparse[(z >> 16) & INTER16L] << 8);
}

/* Deinterleave z into x and y */
static inline void deinterleave_full(uint32_t z, uint16_t *x, uint16_t *y)
{
	*x = deinterleave_half(z);
	*y = deinterleave_half(z>>1);
}

static inline float half_to_lng(uint16_t lng16)
{
	return (((float) lng16) * GEOQUAD_STEP) + LONGITUDE_MIN;
}

static inline float half_to_lat(uint16_t lat16)
{
	return (((float) lat16) * GEOQUAD_STEP) + LATITUDE_MIN;
}

static inline uint16_t lng_to_half(float lng)
{
	return (uint16_t) ((lng - LONGITUDE_MIN) / GEOQUAD_STEP);
}

static inline uint16_t lat_to_half(float lat)
{
	return (uint16_t) ((lat - LATITUDE_MIN) / GEOQUAD_STEP);
}

/***************************
 * DIRECTIONAL FUNCTIONS
 *
 * These all take a qeoquad and return another geoquad north, south, east or
 * west of the given geoquad. These functions are much faster than parsing and
 * recreating a geoquad.
 **************************/

static inline uint32_t quad_northof(uint32_t gq)
{
	uint16_t lng = deinterleave_half(gq >> 1);
	return (gq & INTER32L) | (interleave_half(lng + 1) << 1);
}

static inline uint32_t quad_southof(uint32_t gq)
{
	uint16_t lng = deinterleave_half(gq >> 1);
	return (gq & INTER32L) | (interleave_half(lng - 1) << 1);
}

static inline uint32_t quad_eastof(uint32_t gq)
{
	uint16_t lat = deinterleave_half(gq);
	return (gq & INTER32M) | interleave_half(lat + 1);
}

static inline uint32_t quad_westof(uint32_t gq)
{
	uint16_t lat = deinterleave_half(gq);
	return (gq & INTER32M) | interleave_half(lat - 1);
}

static PyObject*
geoquad_create(PyObject *self, PyObject *args)
{
	uint16_t i, j;
	uint32_t result;
	float lng, lat;
	if (!PyArg_ParseTuple(args, "ff", &lat, &lng))
		return NULL;
	i = (uint16_t) ((lat - LONGITUDE_MIN) / GEOQUAD_STEP);
	j = (uint16_t) ((lng - LATITUDE_MIN) / GEOQUAD_STEP);

	/* yes this is backwards. don't ask */
	result = interleave_full(j, i);
	return PyInt_FromLong((long) result);
}

static PyObject*
geoquad_parse(PyObject *self, PyObject *args)
{
	uint16_t i, j;
	float lng, lat;
	long geoquad;
	PyObject *ret;

	if (!PyArg_ParseTuple(args, "l", &geoquad))
		return NULL;

	if ((ret = PyTuple_New(2)) == NULL)
		return NULL;

	deinterleave_full((uint32_t) geoquad, &i, &j);
	lat = (float) ((i * GEOQUAD_STEP) + LATITUDE_MIN);
	lng = (float) ((j * GEOQUAD_STEP) + LONGITUDE_MIN);

	PyTuple_SetItem(ret, 0, PyFloat_FromDouble((double) lng));
	PyTuple_SetItem(ret, 1, PyFloat_FromDouble((double) lat));
	return ret;
}

/* Define Python functions for northof, southof, eastof, and westof from the
 * corresponding quad_Xof functions.
 */
#define GEOQUAD_DIROF(dir)\
	static PyObject*\
	geoquad_##dir##of(PyObject *self, PyObject *args)\
	{\
		long geoquad;\
		if (!PyArg_ParseTuple(args, "l", &geoquad))\
			return NULL;\
		return PyInt_FromLong((long) quad_##dir##of((uint32_t) geoquad));\
	}
GEOQUAD_DIROF(north)
GEOQUAD_DIROF(south)
GEOQUAD_DIROF(east)
GEOQUAD_DIROF(west)

/*******************************
 * COMPUTING NEARBY GEOQUADS
 *
 * This describes the case of a large circle and small squares (i.e. radius is
 * large compared to any geoquad). This looks like this:
 *
 *
 *      +-------+
 * ---__|       |
 *      |'.     |
 *      |  \    |
 *      +---\---+
 *           .
 *           |
 *           '
 *
 * Now in this situation, the corner of the geoquad is inside of the circle.
 * Since this corner also belongs to the geoquad west and south of the
 * geoquad, those geoquads have at least one point in the circle and therefore
 * they are contained within the circle as well. The geoquads north and east
 * of the geoquad cannot be in the circle.
 *
 * Sometimes two or three corners will in the circle, i.e.
 *
 * ---__                    ----__
 *       '.                  +-----'.+
 *      +--\----+            |       \
 *      |   \   |            |       |\
 *      |    .  |  or        |       | .
 *      |    |  |            +-------+ |
 *      +----'--+                      '
 *          /
 *
 * In these situations three or four of the adjacent neighbors will be within
 * the circle (instead of two neighbors, as above).
 *
 * Using this property, to find the geoquads that are inside of a circle, we
 * do something like this:
 *  1) Find a geoquad along the edge of the circle (by convention, get the one
 *     due north of the circle's center)
 *  2) Travel along the edge filling in edge geoquads
 *  3) Once we have returned to the original geoquad, the circle is complete.
 *     Fill in all of the "missing" geoquads from the interior of the circle.
 */

#if 0
static PyObject*
geoquad_nearby(PyObject *self, PyObject *args)
{
	long geoquad;
	float radius;
	if (!PyArg_ParseTuple(args, "lf", &geoquad, &radius))
		return NULL;
#endif

static PyMethodDef geoquad_methods[] = {
	{ "create", (PyCFunction) geoquad_create, METH_VARARGS, "create a geoquad from a (lng, lat)" },
	{ "parse", (PyCFunction) geoquad_parse, METH_VARARGS, "parse a geoquad, returns a (lng, lat)" },
	{ "northof", (PyCFunction) geoquad_northof, METH_VARARGS, "north of a geoquad, returns a (lng, lat)" },
	{ "southof", (PyCFunction) geoquad_southof, METH_VARARGS, "south of a geoquad, returns a (lng, lat)" },
	{ "eastof", (PyCFunction) geoquad_eastof, METH_VARARGS, "east of a geoquad, returns a (lng, lat)" },
	{ "westof", (PyCFunction) geoquad_westof, METH_VARARGS, "west of a geoquad, returns a (lng, lat)" },
	{ NULL }
};

PyMODINIT_FUNC initgeoquad(void)
{
	PyObject *m = Py_InitModule3("geoquad", geoquad_methods, "test");

	/* TODO: error checking */
	PyObject_SetAttrString(m, "LONGITUDE_MIN", PyFloat_FromDouble(LONGITUDE_MIN));
	PyObject_SetAttrString(m, "LONGITUDE_MAX", PyFloat_FromDouble(LONGITUDE_MAX));
	PyObject_SetAttrString(m, "LATITUDE_MIN", PyFloat_FromDouble(LATITUDE_MIN));
	PyObject_SetAttrString(m, "LATITUDE_MAX", PyFloat_FromDouble(LATITUDE_MAX));
	PyObject_SetAttrString(m, "GEOQUAD_STEP", PyFloat_FromDouble(GEOQUAD_STEP));
}
/* vim: set ts=4 sw=4 tw=78 noet: */
