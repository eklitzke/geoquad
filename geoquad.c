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

static inline uint32_t interleave32(uint16_t x, uint16_t y)
{
	return (
		morton_forward[y >> 8]   << 17 |
		morton_forward[x >> 8]   << 16 |
		morton_forward[y & 0xFF] << 1  |
		morton_forward[x & 0xFF]);
}

static inline void deinterleave32(uint32_t z, uint16_t *x, uint16_t *y)
{
	uint16_t half = z & 0xFFFF;
	*x = morton_sparse[half & 0x5555];
	half >>= 1;
	*y = (morton_sparse[half & 0x5555]);
	half = z >> 16;
	*x |= (morton_sparse[half & 0x5555]) << 8;
	half >>= 1;
	*y |= ((morton_sparse[half & 0x5555]) << 8);
}

static PyObject*
geoquad_create(PyObject *self, PyObject *args)
{
	uint16_t i, j;
	uint32_t result;
	float lng, lat;
	if (!PyArg_ParseTuple(args, "ff", &lng, &lat))
		return NULL;
	i = (uint16_t) ((lng - LONGITUDE_MIN) / GEOQUAD_STEP);
	j = (uint16_t) ((lat - LATITUDE_MIN) / GEOQUAD_STEP);

	/* yes this is backwards. don't ask */
	result = interleave32(j, i);
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

	deinterleave32((uint32_t) geoquad, &i, &j);
	lat = (float) ((i * GEOQUAD_STEP) + LATITUDE_MIN);
	lng = (float) ((j * GEOQUAD_STEP) + LONGITUDE_MIN);

	PyTuple_SetItem(ret, 0, PyFloat_FromDouble((double) lng));
	PyTuple_SetItem(ret, 1, PyFloat_FromDouble((double) lat));
	return ret;
}

static PyMethodDef geoquad_methods[] = {
	{ "create", (PyCFunction) geoquad_create, METH_VARARGS, "create a geoquad from a (lng, lat)" },
	{ "parse", (PyCFunction) geoquad_parse, METH_VARARGS, "parse a geoquad, returns a (lng, lat)" },
	{ NULL }
};

PyMODINIT_FUNC initgeoquad(void)
{
	PyObject *m = Py_InitModule3("geoquad", geoquad_methods, "test");
}
/* vim: set ts=4 sw=4 tw=78 noet: */
