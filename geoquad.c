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

static inline uint32_t quad_northof(uint32_t gq)
{
	float lat = half_to_lat(deinterleave_half(gq)) + GEOQUAD_STEP;
	return (gq & INTER32M) | interleave_half(lat_to_half(lat));
}

static inline uint32_t quad_southof(uint32_t gq)
{
	float lat = half_to_lat(deinterleave_half(gq)) - GEOQUAD_STEP;
	return (gq & INTER32M) | interleave_half(lat_to_half(lat));
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

static PyObject*
geoquad_northof(PyObject *self, PyObject *args)
{
	long geoquad;
	if (!PyArg_ParseTuple(args, "l", &geoquad))
		return NULL;
	return PyInt_FromLong((long) quad_northof((uint32_t) geoquad));
}

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
	//{ "southof", (PyCFunction) quad_southof, METH_VARARGS, "south of a geoquad, returns a (lng, lat)" },
	{ NULL }
};

PyMODINIT_FUNC initgeoquad(void)
{
	PyObject *m = Py_InitModule3("geoquad", geoquad_methods, "test");
}
/* vim: set ts=4 sw=4 tw=78 noet: */
