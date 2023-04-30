#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "Python.h"
#include "../linker_js.h"

PyObject *linker_buffer_to_py_buffer(linker_buffer buf)
{
  Py_buffer py_buf;
  Py_ssize_t shape[] = {buf.len};
  PyBuffer_FillInfo(&py_buf, NULL, buf.data, buf.len, 0, PyBUF_WRITABLE);
  py_buf.format = "B";
  py_buf.shape = shape;
  return PyMemoryView_FromBuffer(&py_buf);
}

void call_fn(char *lib_dir, char *lib_name, char *fn_name, u_int8_t args_len, linker_value args[ARG_MAX], linker_type return_type, void *result)
{
  Py_Initialize();

  PyObject *sys_path = PySys_GetObject("path");
  PyObject *module_path = PyUnicode_FromString(lib_dir);
  PyList_Append(sys_path, module_path);
  Py_DECREF(module_path);

  PyObject *module_name = PyUnicode_FromString(lib_name);
  PyObject *module = PyImport_Import(module_name);
  Py_DECREF(module_name);

  if (module == NULL)
  {
    PyErr_Print();
    result = NULL;
    Py_DECREF(module_path);
    return;
  }

  PyObject *fn = PyObject_GetAttrString(module, fn_name);

  if (!fn || !PyCallable_Check(fn))
  {
    PyErr_Print();
    result = NULL;
    Py_XDECREF(fn);
    Py_DECREF(module);
    Py_DECREF(module_path);
    return;
  }

  PyObject *py_args = PyTuple_New(args_len);

  for (uint32_t i = 0; i < args_len; i++)
  {
    linker_value arg = args[i];

    PyObject *py_arg;

    switch (arg.type)
    {
    case linker_type_buffer:
    {
      py_arg = linker_buffer_to_py_buffer(*(linker_buffer *)(arg.value));
      break;
    }

    case linker_type_bool:
    {
      py_arg = PyBool_FromLong(*(bool *)arg.value);
      break;
    }

    case linker_type_number:
    {
      py_arg = PyFloat_FromDouble(*(double *)arg.value);
      break;
    }

    case linker_type_string:
    {
      py_arg = PyUnicode_FromString((char *)arg.value);
      break;
    }
    }

    PyTuple_SetItem(py_args, i, py_arg);
  }

  PyObject *py_result = PyObject_CallObject(fn, py_args);

  switch (return_type)
  {
  case linker_type_buffer:
  {
    linker_buffer *buf = result;
    Py_buffer py_buf;
    PyObject_GetBuffer(py_result, &py_buf, PyBUF_WRITABLE);
    buf->data = py_buf.buf;
    buf->len = py_buf.len;
    Py_DECREF(&(py_buf.buf));
    break;
  }

  case linker_type_bool:
  {
    *((bool *)result) = PyFloat_AsDouble(py_result);
    break;
  }

  case linker_type_number:
  {
    *((double *)result) = PyFloat_AsDouble(py_result);
    break;
  }

  case linker_type_string:
  {
    const char *str = PyUnicode_AsUTF8(py_result);
    void **out = result;
    *out = malloc(strlen(str) + 1);
    strcpy(*out, str);
    break;
  }
  }

  Py_DECREF(py_args);
  Py_DECREF(py_result);
  Py_XDECREF(fn);
  Py_DECREF(module);
  Py_DECREF(module_path);
  Py_Finalize();
}
