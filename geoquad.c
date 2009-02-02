#include <Python.h>

#include "data.h"
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <assert.h>

#include <stdlib.h>

#define LONGITUDE_MIN  -180.0
#define LONGITUDE_MAX   180.0
#define LATITUDE_MIN    -90.0
#define LATITUDE_MAX     90.0

#define EARTH_RADIUS_MI 3958.8641024047724

#define MILES_PER_LATITUDE 68.70795454545454

/* Unfortunately, in C we have (1 / 0.05 ) != 20
 * This causes incompatibilites with the current Python code.
 */
#define GEOQUAD_STEP     0.05
#define GEOQUAD_INV      20
#define GEOQUAD_FUZZ     (GEOQUAD_STEP * 0.70710678118654757)

/* Interleaved ones and zeroes, LSB = 1 */
#define INTER16L 0x5555
#define INTER32L 0x55555555

/* Interleaved ones and zeroes, MSB = 1 */
#define INTER16M 0xAAAA
#define INTER32M 0xAAAAAAAA

#define TO_RADIANS(x)   (x * M_PI / 180.0)

/* A half interleave/ */
static const inline uint32_t interleave_half(uint16_t x)
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

/* TODO: there's something fishy about these functions... */
static inline double half_to_lng(uint16_t lng16)
{
	return (lng16 * GEOQUAD_STEP) + (LONGITUDE_MIN / 2);
}

static inline double half_to_lat(uint16_t lat16)
{
	return (lat16 * GEOQUAD_STEP) + (LATITUDE_MIN * 2);
}

static inline uint16_t lng_to_half(double lng)
{
	return (uint16_t) ((lng - LONGITUDE_MIN) * GEOQUAD_INV);
}

static inline uint16_t lat_to_half(double lat)
{
	return (uint16_t) ((lat - LATITUDE_MIN) * GEOQUAD_INV);
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
	uint16_t normal_lat, normal_lng;
	uint32_t result;
	char *err_msg;
	double lng, lat;

	if (!PyArg_ParseTuple(args, "dd", &lat, &lng))
		return NULL;

	if ((lat < LATITUDE_MIN) || (lat > LATITUDE_MAX)) {
		if (!(err_msg = PyMem_Malloc(128)))
			return PyErr_NoMemory();
		sprintf(err_msg, "Invalid latitude (%1.2f); should be in range [%3.1f, %3.1f]", lat, LATITUDE_MIN, LATITUDE_MAX);
		PyErr_SetString(PyExc_ValueError, err_msg);
		PyMem_Free(err_msg);
		return NULL;
	}
	if ((lng < LONGITUDE_MIN) || (lng > LONGITUDE_MAX)) {
		if (!(err_msg = PyMem_Malloc(128)))
			return PyErr_NoMemory();
		sprintf(err_msg, "Invalid longitude (%1.2f); should be in range [%3.1f, %3.1f]", lng, LONGITUDE_MIN, LONGITUDE_MAX);
		PyErr_SetString(PyExc_ValueError, err_msg);
		PyMem_Free(err_msg);
		return NULL;
	}
	normal_lat = (uint16_t) ((lat - LATITUDE_MIN) * GEOQUAD_INV);
	normal_lng = (uint16_t) ((lng - LONGITUDE_MIN) * GEOQUAD_INV);

	result = interleave_full(normal_lat, normal_lng);
	return PyInt_FromLong((long) result);
}

static PyObject*
geoquad_parse(PyObject *self, PyObject *args)
{
	uint16_t half_lat, half_lng;
	double lng, lat;
	long geoquad;
	PyObject *ret;

	if (!PyArg_ParseTuple(args, "l", &geoquad))
		return NULL;

	if ((ret = PyTuple_New(2)) == NULL)
		return NULL;

	deinterleave_full((uint32_t) geoquad, &half_lat, &half_lng);
	lat = ((half_lat * GEOQUAD_STEP) + LATITUDE_MIN);
	lng = ((half_lng * GEOQUAD_STEP) + LONGITUDE_MIN);

	PyTuple_SetItem(ret, 0, PyFloat_FromDouble(lat));
	PyTuple_SetItem(ret, 1, PyFloat_FromDouble(lng));
	return ret;
}

static PyObject*
geoquad_center(PyObject *self, PyObject *args)
{
	uint16_t half_lat, half_lng;
	double lng, lat;
	long geoquad;
	PyObject *ret;

	if (!PyArg_ParseTuple(args, "l", &geoquad))
		return NULL;

	if ((ret = PyTuple_New(2)) == NULL)
		return NULL;

	deinterleave_full((uint32_t) geoquad, &half_lat, &half_lng);
	lat = ((half_lat * GEOQUAD_STEP) + LATITUDE_MIN) + GEOQUAD_STEP / 2;
	lng = ((half_lng * GEOQUAD_STEP) + LONGITUDE_MIN) + GEOQUAD_STEP / 2;

	PyTuple_SetItem(ret, 0, PyFloat_FromDouble(lat));
	PyTuple_SetItem(ret, 1, PyFloat_FromDouble(lng));
	return ret;
}

static PyObject*
geoquad_contains(PyObject *self, PyObject *args)
{
	uint16_t half_lat, half_lng;
	const long geoquad;
	const double in_lng, in_lat;
	double lng, lat;
	PyObject *ret;

	if (!PyArg_ParseTuple(args, "ldd", &geoquad, &in_lat, &in_lng))
		return NULL;

	if ((ret = PyTuple_New(2)) == NULL)
		return NULL;

	deinterleave_full((uint32_t) geoquad, &half_lat, &half_lng);
	lat = ((half_lat * GEOQUAD_STEP) + LATITUDE_MIN);
	lng = ((half_lng * GEOQUAD_STEP) + LONGITUDE_MIN);

	return PyBool_FromLong((lat <= in_lat) && ((lat + GEOQUAD_STEP) > in_lat) && (lng <= in_lng) && ((lng + GEOQUAD_STEP) > in_lng));
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

/* This creates a Python list object containing a list of geoquads. The
 * interpretation of the return result and of the arguments is as follows:
 *
 * Suppose we have a circle like this:   ###
 *                                      #   #  <----- @top
 *                                      #   #  <----- @bot
 *                                       ###
 * @halves should be an array where the first half is the latitudes of the
 * geoquads on the top half of the circle, the second half is the latitudes of
 * the geoquads on the bottom half of the circle.If there is an odd number of
 * geoquads north to south then at least one geoquad should be in both halves
 * of @halves;
 *
 * This function goes through and creates a Python list object containing all
 * of the geoquads in the circle by using the fast quad_southof function.
 */
static PyObject*
fill_nearby_list(uint16_t halves[], uint16_t lng_w, size_t len)
{
	int i;
	uint16_t t, b;
	uint32_t q;
	uint16_t lng;
	PyObject *g, *gs;
	gs = PyList_New(0); /* FIXME */

	lng = lng_w;
	for (i = 0; i < len; i++) {
		t = halves[i];
		b = halves[len + i];

		q = interleave_full(lng, t);

		/* Append the top geoquad to the list. */
		if (!(g = PyInt_FromLong((long) q)))
			return NULL;
		if (PyList_Append(gs, g))
			return NULL;

		while (t > b) {

			/* This computes the geoquad south of q */
			q &= INTER32L;
			t--;
			q |= (interleave_half(t) << 1);

			/* Add the geoquad to our list */
			if (!(g = PyInt_FromLong((long) q)))
				return NULL;
			if (PyList_Append(gs, g))
				return NULL;
		}

		/* Move one quad east */
		lng++;
	}
	return gs;
}

/* FIXME: too many arguments */
static inline int
quad_within_radius(double lat, double lng, double lat_c, double lng_c, double radius_sq)
{
	double delta_lat, delta_lng;
	delta_lat = lat - lat_c;
	delta_lng = lng - lng_c;
	return (delta_lat * delta_lat + delta_lng * delta_lng) <= radius_sq;
}

static PyObject*
geoquad_nearby(PyObject *self, PyObject *args, PyObject *kw)
{
	long geoquad;
	double radius;
	double radius_sq;
	double f_lng_orig, f_lat_orig, f_lng, f_lat;
	uint16_t lng_w, lng_e;
	uint16_t lng, lat, lng_orig, lat_orig;
	size_t i, count;
	int fuzz = 0;
	PyObject *ret;
	uint16_t *halves;

	static char *kwlist[] = {"geoquad", "radius", "fuzz", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kw, "ld|i", kwlist, &geoquad, &radius, &fuzz))
		return NULL;

	radius /= MILES_PER_LATITUDE;

	/* If the fuzz parameter evaluates to True, then the radius is
	 * automatically "fuzzed" by making it incrementally bigger. It will be
	 * fuzzed by the right amount so that any edge effects on the circle will
	 * be handled correctly.
	 */
	if (fuzz)
		radius += GEOQUAD_FUZZ;

	radius_sq = radius * radius;
	
	/* Parse the geoquad into a lng, lat and compute the easternmost
	 * encompassing geoquad.
	 *
	 * FIXME: we might be off by one w/o the lat/lng conversion, is there a
	 * way to fix that? Skipping it would be faster. */

	deinterleave_full((uint32_t) geoquad, &lng, &lat);
	lat_orig = lat;
	lng_orig = lng;

	f_lng_orig = half_to_lng(lng);
	f_lat_orig = half_to_lat(lat);

	/* Get the westernmost geoquad */
	lng_w = lng - (uint16_t) ceil(radius / GEOQUAD_STEP);

	f_lng = half_to_lng(lng_w);

#if 0
	while ((f_lng - 0.5 * GEOQUAD_STEP) > (f_lng_orig - radius)) {
		lng_w--;
		f_lng = half_to_lng(lng_w);
	}
#endif

	/* Get the easternmost quad */
	lng_e = lng + (uint16_t) floor(radius / GEOQUAD_STEP);

	f_lng = half_to_lng(lng_e);

#if 0
	while ((f_lng + 0.5 * GEOQUAD_STEP) < (f_lng_orig + radius)) {
		lng_e++;
		f_lng = half_to_lng(lng_e);
	}
#endif

	count = lng_e - lng_w + 1;

	halves = PyMem_Malloc(sizeof(uint16_t) * (count << 1));
	if (halves == NULL)
		return PyErr_NoMemory();

	i = 0;
	for (lng = lng_w; lng <= lng_e; lng++) {
		lat = lat_orig;
		f_lng = half_to_lng(lng);
		f_lat = half_to_lat(lat);

		while (quad_within_radius(f_lat + (GEOQUAD_STEP / 2), f_lng + (GEOQUAD_STEP / 2), f_lat_orig, f_lng_orig, radius_sq)) {
			lat++;
			f_lat = half_to_lat(lat);
		}
		lat--;
		halves[i] = lat;

		lat = lat_orig;
		f_lat = half_to_lat(lat);
		while (quad_within_radius(f_lat + (GEOQUAD_STEP / 2), f_lng + (GEOQUAD_STEP / 2), f_lat_orig, f_lng_orig, radius_sq)) {
			lat--;
			f_lat = half_to_lat(lat);
		}
		lat++;
		halves[i + count] = lat;
		i++;
	}

	ret = fill_nearby_list(halves, lng_w, count);
	PyMem_Free(halves);

#ifdef DEBUG
	if (PyList_Sort(ret) == -1)
		return NULL;
#endif

	return ret;
}

const static double haversine_distance(double lat1, double lng1, double lat2, double lng2)
{
	double shlat, shlng;

	lng1 = TO_RADIANS(lng1);
	lat1 = TO_RADIANS(lat1);
	lng2 = TO_RADIANS(lng2);
	lat2 = TO_RADIANS(lat2);

	shlat = sin((lat2 - lat1) / 2.0);
	shlng = sin((lng2 - lng1) / 2.0);

	return EARTH_RADIUS_MI * 2.0 * asin(fmin(1.0, sqrt(shlat * shlat + cos(lat1) * cos(lat2) * shlng * shlng)));
}

static PyObject*
geoquad_haversine_distance(PyObject *self, PyObject *args)
{
	PyObject *t1, *t2;
	double lat1, lat2, lng1, lng2;

	if (!PyArg_ParseTuple(args, "O!O!", &PyTuple_Type, &t1, &PyTuple_Type, &t2))
		return NULL;

	if (PyTuple_GET_SIZE(t1) != 2) {
		PyErr_SetString(PyExc_TypeError, "First argument was not a tuple of length two");
		return NULL;
	}
	if (PyTuple_GET_SIZE(t2) != 2) {
		PyErr_SetString(PyExc_TypeError, "Second argument was not a tuple of length two");
		return NULL;
	}

	lat1 = PyFloat_AsDouble(PyTuple_GET_ITEM(t1, 0));
	lng1 = PyFloat_AsDouble(PyTuple_GET_ITEM(t1, 1));
	lat2 = PyFloat_AsDouble(PyTuple_GET_ITEM(t2, 0));
	lng2 = PyFloat_AsDouble(PyTuple_GET_ITEM(t2, 1));

	return PyFloat_FromDouble(haversine_distance(lat1, lng1, lat2, lng2));
}

static PyMethodDef geoquad_methods[] = {
	{ "create", (PyCFunction) geoquad_create, METH_VARARGS, "create a geoquad from a (lat, lng)" },
	{ "parse", (PyCFunction) geoquad_parse, METH_VARARGS, "SW corner of a geoquad, returns a (lat, lng)" },
	{ "center", (PyCFunction) geoquad_center, METH_VARARGS, "center of a geoquad, returns a (lat, lng)" },
	{ "contains", (PyCFunction) geoquad_contains, METH_VARARGS, "whether or not a geoquad contaings a lng, lat" },
	{ "northof", (PyCFunction) geoquad_northof, METH_VARARGS, "returns the geoquad directly north of a given geoquad" },
	{ "southof", (PyCFunction) geoquad_southof, METH_VARARGS, "returns the geoquad directly south of a given geoquad" },
	{ "eastof", (PyCFunction) geoquad_eastof, METH_VARARGS, "returns the geoquad directly east of a given geoquad" },
	{ "westof", (PyCFunction) geoquad_westof, METH_VARARGS, "returns the geoquad directly west of a given geoquad" },
	{ "nearby", (PyCFunction) geoquad_nearby, METH_VARARGS|METH_KEYWORDS, "get nearby geoquads, returns a list of geoquads" },
	{ "haversine_distance", (PyCFunction) geoquad_haversine_distance, METH_VARARGS, "haversine distance" },
	{ NULL }
};

PyMODINIT_FUNC initgeoquad(void)
{
	PyObject *m = Py_InitModule3("geoquad", geoquad_methods, "test");

	/* TODO: There should be error checking here, but I can't figure out how
	 * to signal failure from a module's init method... */
	PyObject_SetAttrString(m, "LONGITUDE_MIN", PyFloat_FromDouble(LONGITUDE_MIN));
	PyObject_SetAttrString(m, "LONGITUDE_MAX", PyFloat_FromDouble(LONGITUDE_MAX));
	PyObject_SetAttrString(m, "LATITUDE_MIN", PyFloat_FromDouble(LATITUDE_MIN));
	PyObject_SetAttrString(m, "LATITUDE_MAX", PyFloat_FromDouble(LATITUDE_MAX));
	PyObject_SetAttrString(m, "MILES_PER_LATITUDE", PyFloat_FromDouble(MILES_PER_LATITUDE));
	PyObject_SetAttrString(m, "GEOQUAD_STEP", PyFloat_FromDouble(GEOQUAD_STEP));
	PyObject_SetAttrString(m, "GEOQUAD_INV", PyFloat_FromDouble(GEOQUAD_INV));
	PyObject_SetAttrString(m, "GEOQUAD_FUZZ", PyFloat_FromDouble(GEOQUAD_FUZZ));
}
/* vim: set ts=4 sw=4 tw=78 noet: */
