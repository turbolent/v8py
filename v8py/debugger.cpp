#include <Python.h>
#include "v8py.h"
#include <v8.h>
#if defined(V8_OS_LINUX)
# include <arpa/inet.h>
#endif
#include "context.h"
#include "debugger.h"

#ifdef _WIN32

// don't want to include Winsock2.h
uint16_t htons (uint16_t x)
{
#if BYTE_ORDER == BIG_ENDIAN
  return x;
#elif BYTE_ORDER == LITTLE_ENDIAN
  return __bswap_16 (x);
#else
# error "What kind of system is this?"
#endif
}

#endif

PyObject *json_from_stringview(const std::unique_ptr<StringBuffer> &message);
std::unique_ptr<StringBuffer> stringview_from_json(PyObject *json);

PyMethodDef debugger_methods[] = {
    {"send", (PyCFunction) debugger_send, METH_O, NULL},
    {NULL},
};
PyTypeObject debugger_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
};
int debugger_type_init() {
    debugger_type.tp_name = "v8py.Debugger";
    debugger_type.tp_basicsize = sizeof(debugger_c);
    debugger_type.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
    debugger_type.tp_doc = "";
    debugger_type.tp_dealloc = (destructor) debugger_dealloc;
    debugger_type.tp_new = (newfunc) debugger_new;
    debugger_type.tp_init = (initproc) debugger_init;
    debugger_type.tp_methods = debugger_methods;
    return PyType_Ready(&debugger_type);
}

PyObject *debugger_new(PyTypeObject *type, PyObject *args, PyObject *kwargs) {
    debugger_c *self = (debugger_c *) type->tp_alloc(type, 0);
    PyErr_PROPAGATE(self);
    self->client = NULL; // indicate the object has not been initialized
    return (PyObject *) self;
}

int debugger_init(debugger_c *self, PyObject *args, PyObject *kwargs) {
    IN_V8;

    context_c *context;
    if (PyArg_ParseTuple(args, "O", &context) < 0) {
        return -1;
    }

    Py_INCREF(context);
    self->context = context;
    Local<Context> js_context = context->js_context.Get(isolate);

    if (context->has_debugger) {
        PyErr_SetString(PyExc_ValueError, "specified context already has a debugger attached");
        return 0;
    }
    context->has_debugger = true;

    self->client = new V8PyInspectorClient(self);
    self->inspector = V8Inspector::create(isolate, self->client);
    self->inspector->contextCreated(V8ContextInfo(js_context, 1, StringView()));
    self->channel = new V8PyChannel(self);
    self->session = self->inspector->connect(1, self->channel, StringView());

    return 0;
}

void call_self(debugger_c *self, const char *method) {
    PyObject *debugger = (PyObject *) self;
    if (PyObject_HasAttrString(debugger, method)) {
        if (PyObject_CallMethod(debugger, (char *) method, NULL) == NULL) {
            PyErr_WriteUnraisable(debugger);
        }
    }
}

void V8PyChannel::handle_message(const std::unique_ptr<StringBuffer> &message) {
    PyObject *json = json_from_stringview(message);
    if (json == NULL) {
        PyErr_WriteUnraisable((PyObject *) debugger_);
        return;
    }
    PyObject *self = (PyObject *) debugger_;
    if (PyObject_HasAttrString(self, "handle")) {
        if (PyObject_CallMethod(self, (char *) "handle", (char *) "O", json) == NULL) {
            PyErr_WriteUnraisable(self);
        }
    }
}

void V8PyInspectorClient::runMessageLoopOnPause(int context_group_id) {
    call_self(debugger_, "run_loop");
}

void V8PyInspectorClient::quitMessageLoopOnPause() {
    call_self(debugger_, "quit_loop");
}

PyObject *debugger_send(debugger_c *self, PyObject *message) {
    if (self->client == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "__init__ has not yet been called");
        return NULL;
    }

    IN_V8;
    std::unique_ptr<StringBuffer> stringview = stringview_from_json(message);
    if (stringview.get() == NULL) return NULL;
    self->session.get()->dispatchProtocolMessage(stringview.get()->string());
    Py_RETURN_NONE;
}

void debugger_dealloc(debugger_c *self) {
    IN_V8;
    self->session.reset();
    self->inspector.reset();
    delete self->client;
    delete self->channel;
    self->context->has_debugger = false;
    Py_DECREF(self->context);
    Py_TYPE(self)->tp_free((PyObject *) self);
}

PyObject *json_from_stringview(const std::unique_ptr<StringBuffer> &message) {
    static PyObject *loads = NULL;
    if (loads == NULL) {
        PyObject *json = PyImport_ImportModule("json");
        PyErr_PROPAGATE(json);
        loads = PyObject_GetAttrString(json, "loads");
        PyErr_PROPAGATE(loads);
    }

    const StringView &stringview = message->string();
    PyObject *string;
    if (stringview.is8Bit()) {
        string = PyUnicode_DecodeLatin1((const char *) stringview.characters8(), stringview.length(), NULL);
    } else {
        string = PyUnicode_DecodeUTF16((const char *) stringview.characters16(), stringview.length() * 2, NULL, NULL);
    }
    PyErr_PROPAGATE(string);
    return PyObject_CallFunctionObjArgs(loads, string, NULL);
}

std::unique_ptr<StringBuffer> stringview_from_json(PyObject *json) {
    static PyObject *dumps = NULL;
    if (dumps == NULL) {
        PyObject *json = PyImport_ImportModule("json");
        PyErr_PROPAGATE(json);
        dumps = PyObject_GetAttrString(json, "dumps");
        PyErr_PROPAGATE(dumps);
    }

    PyObject *string = PyObject_CallFunctionObjArgs(dumps, json, NULL);
    PyErr_PROPAGATE(string);
    PyObject *utf16_bytes = PyUnicode_AsUTF16String(string);
    PyErr_PROPAGATE(utf16_bytes);
    Py_DECREF(string);
    const uint16_t *command_bytes = (const uint16_t *) PyBytes_AS_STRING(utf16_bytes);
    int length = PyBytes_GET_SIZE(utf16_bytes)/2; // it wants the number of characters, not the number of bytes
    return StringBuffer::create(StringView(command_bytes, length));
}
