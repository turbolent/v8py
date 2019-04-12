#include <Python.h>
#include "v8py.h"
#include <v8.h>

#include "convert.h"
#include "pyfunction.h"
#include "pyclass.h"
#include "jsobject.h"
#include "context.h"

PyObject *py_from_js(Local<Value> value, Local<Context> context) {
    IN_V8;

    if (value->IsSymbol()) {
        value = value.As<Symbol>()->Name();
    }

    if (value->IsNull()) {
        Py_INCREF(null_object);
        return null_object;
    }
    if (value->IsUndefined()) {
        Py_INCREF(Py_None);
        return Py_None;
    }

    if (value->IsBoolean()) {
        Local<Boolean> bool_value = value.As<Boolean>();
        if (bool_value->Value()) {
            Py_INCREF(Py_True);
            return Py_True;
        } else {
            Py_INCREF(Py_False);
            return Py_False;
        }
    }

    if (value->IsArray()) {
        Local<Array> array = value.As<Array>();
        PyObject *list = PyList_New(array->Length());
        for (uint32_t i = 0; i < array->Length(); i++) {
            PyObject *obj = py_from_js(array->Get(context, i).ToLocalChecked(), context);
            if (obj == NULL) {
                Py_DECREF(list);
                return NULL;
            }
            PyList_SET_ITEM(list, i, obj);
        }
        return list;
    }

    if (value->IsObject()) {
        Local<Object> obj_value = value.As<Object>();
        if (context.IsEmpty()) {
            context = obj_value->CreationContext();
        }
        if (obj_value->GetPrototype()->StrictEquals(context->GetEmbedderData(OBJECT_PROTOTYPE_SLOT))) {
            PyObject *dict = PyDict_New();
            PyErr_PROPAGATE(dict);
            Local<Array> js_keys = obj_value->GetPropertyNames(context).ToLocalChecked();
            uint32_t length = js_keys->Length();
            for (uint32_t i = 0; i < length; i++) {
                Local<Value> js_key = js_keys->Get(context, i).ToLocalChecked();
                PyObject *key = py_from_js(js_key, context);
                if (key == NULL) {
                    Py_DECREF(dict);
                    return NULL;
                }
                PyObject *value = py_from_js(obj_value->Get(context, js_key).ToLocalChecked(), context);
                if (value == NULL) {
                    Py_DECREF(dict);
                    Py_DECREF(key);
                    return NULL;
                }
                if (PyDict_SetItem(dict, key, value) < 0) {
                    Py_DECREF(dict);
                    Py_DECREF(key);
                    Py_DECREF(value);
                    return NULL;
                }
            }
            return dict;
        }
        if (obj_value->InternalFieldCount() == OBJECT_INTERNAL_FIELDS) {
            Local<Value> magic = obj_value->GetInternalField(0);
            if (magic == IZ_DAT_OBJECT) {
                PyObject *object = (PyObject *) obj_value->GetInternalField(1).As<External>()->Value();
                Py_INCREF(object);
                return object;
            }
        }
        return (PyObject *) js_object_new(obj_value, context);
    }

    if (value->IsString()) {
        Local<String> str_value = value.As<String>();
        size_t bufsize = str_value->Length() * sizeof(uint16_t);

        PyObject *py_value;

        if (bufsize <= STRING_BUFFER_SIZE) {
            str_value->Write(isolate, &string_buffer[0], 0, -1, String::WriteOptions::NO_NULL_TERMINATION);
            py_value = PyUnicode_DecodeUTF16((const char *) &string_buffer[0], bufsize, NULL, NULL);
        } else {
            uint16_t *buf = (uint16_t *) malloc(bufsize);
            PyErr_PROPAGATE(buf);
            str_value->Write(isolate, buf, 0, -1, String::WriteOptions::NO_NULL_TERMINATION);
            py_value = PyUnicode_DecodeUTF16((const char *) buf, bufsize, NULL, NULL);
            free(buf);
        }

        return py_value;
    }
    if (value->IsUint32() || value->IsInt32()) {
        return PyLong_FromLongLong((PY_LONG_LONG) value.As<Integer>()->Value());
    }
    if (value->IsNumber()) {
        return PyFloat_FromDouble(value.As<Number>()->Value());
    }

    // i'm not quite ready to turn this into an assert quite yet
    // i'll do that when I've special-cased every primitive type
    printf("cannot convert\n");
    Py_RETURN_NONE;
}

Local<Value> js_from_py(PyObject *value, Local<Context> context) {
    ESCAPING_IN_V8;

    if (value == Py_False) {
        return hs.Escape(False(isolate));
    }
    if (value == Py_True) {
        return hs.Escape(True(isolate));
    }
    if (value == Py_None) {
        return hs.Escape(Undefined(isolate));
    }
    if (value == null_object) {
        return hs.Escape(Null(isolate));
    }

    if (PyBytes_Check(value)) {
        Py_buffer view;
        int error = PyObject_GetBuffer(value, &view, PyBUF_SIMPLE);
        Local<ArrayBuffer> js_value = ArrayBuffer::New(isolate, view.len);
        memcpy(js_value->GetContents().Data(), view.buf, view.len);
        PyBuffer_Release(&view);
        return hs.Escape(js_value);
    }

    if (PyUnicode_Check(value)) {
        Py_ssize_t len;
        const char *str = PyUnicode_AsUTF8AndSize(value, &len);
        Local<String> js_value = String::NewFromUtf8(isolate, str, NewStringType::kNormal, len).ToLocalChecked();
        return hs.Escape(js_value);
    }

    if (PyNumber_Check(value)) {
        Local<Number> js_value;
        if (PyFloat_Check(value)) {
            js_value = Number::New(isolate, PyFloat_AS_DOUBLE(value));
        } else if (PyLong_Check(value)) {
            js_value = Integer::New(isolate, PyLong_AsLong(value));
        } else {
            // TODO make this work right
            printf("what the hell kind of number is this?!");
            return hs.Escape(Undefined(isolate));
        }
        return hs.Escape(js_value);
    }

    if (PyDict_Check(value)) {
        // a context scope is (I think) needed for Object::New to work
        Context::Scope cs(context);
        Local<Object> js_dict = Object::New(isolate);

        PyObject *dict = value;
        PyObject *key, *value;
        Py_ssize_t pos = 0;
        while (PyDict_Next(dict, &pos, &key, &value)) {
            js_dict->Set(context, js_from_py(key, context), js_from_py(value, context)).FromJust();
        }
        return hs.Escape(js_dict);
    }

    if (PyList_Check(value) || PyTuple_Check(value)) {
        int length = PySequence_Length(value);
        Local<Array> array = Array::New(isolate, length);
        for (int i = 0; i < length; i++) {
            PyObject *item = PySequence_ITEM(value, i);
            bool set_worked = array->Set(context, i, js_from_py(item, context)).FromJust();
            assert(set_worked);
            Py_DECREF(item);
        }
        return hs.Escape(array);
    }

    if (PyFunction_Check(value)) {
        py_function *templ = (py_function *) py_function_to_template(value);
        return hs.Escape(py_template_to_function(templ, context));
    }

    if (PyType_Check(value)) {
        py_class *templ = (py_class *) py_class_to_template(value);
        return hs.Escape(py_class_get_constructor(templ, context));
    }

    if (PyObject_TypeCheck(value, &js_object_type)) {
        js_object *py_value = (js_object *) value;
        return hs.Escape(py_value->object.Get(isolate));
    }

    // it's an arbitrary object
    PyObject *type;
    type = (PyObject *) Py_TYPE(value);
    Py_INCREF(type);
    py_class *templ = (py_class *) py_class_to_template(type);
    Py_DECREF(type);
    return hs.Escape(py_class_create_js_object(templ, value, context));
}

PyObject *pys_from_jss(const FunctionCallbackInfo<Value> &js_args, Local<Context> context) {
    PyObject *py_args = PyTuple_New(js_args.Length());
    PyErr_PROPAGATE(py_args);
    for (int i = 0; i < js_args.Length(); i++) {
        PyObject *arg = py_from_js(js_args[i], context);
        if (arg == NULL) {
            Py_DECREF(py_args);
            return NULL;
        }
        PyTuple_SET_ITEM(py_args, i, arg);
    }
    return py_args;
}

// js_args is an out parameter, expected to contain enough space
void jss_from_pys(PyObject *py_args, Local<Value> *js_args, Local<Context> context) {
    int size = PyTuple_GET_SIZE(py_args);
    for (int i = 0; i < size; i++) {
        js_args[i] = js_from_py(PyTuple_GET_ITEM(py_args, i), context);
    }
}

