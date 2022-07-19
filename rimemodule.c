#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <rime_api.h>
#include <stdbool.h>

static bool initialized = false;

static PyObject*
newref(PyObject* obj) {
    Py_INCREF(obj);
    return obj;
}

static PyObject*
rime_init(PyObject* self, PyObject* args) {
    const bool reinitialize;
    if(!PyArg_ParseTuple(args, "p", &reinitialize)) {
        Py_RETURN_NONE;
    }

    if (initialized) {
        if (reinitialize) {
            RimeFinalize(); 
            initialized = false;
        } else {
            Py_RETURN_TRUE;
        }
    }

    RIME_STRUCT(RimeTraits, rime_traits);
    rime_traits.distribution_name = "Rime";
    rime_traits.distribution_code_name = "pyrime";
    rime_traits.distribution_version = "pyrime";
    rime_traits.app_name = "rime.pyrime";

    bool error = false;
    PyObject* unicodeString;
    PyObject* encodedString;

#define set_traints(property) unicodeString = PyObject_GetAttrString(self, #property); \
    if (unicodeString) { \
        encodedString = PyUnicode_AsEncodedString(unicodeString, "UTF-8", "strict"); \
        if (encodedString) { \
            char* cstring = strdup(PyBytes_AsString(encodedString)); \
            rime_traits.property = cstring; \
            Py_DECREF(encodedString); \
        } else { \
            error = true; \
        } \
        Py_DECREF(unicodeString); \
    } else { \
        error = true; \
    } \
    if (error) { \
        Py_RETURN_FALSE; \
    }

    set_traints(shared_data_dir)
    set_traints(user_data_dir)
    set_traints(log_dir)

    RimeSetup(&rime_traits);
    RimeInitialize(&rime_traits);
    RimeStartMaintenance(false);

    initialized = true;
    Py_RETURN_TRUE;
}

static PyObject*
rime_get_candidates_from_keys(PyObject* self, PyObject* args) {

    if (!initialized) {
        Py_RETURN_NONE;
    }

    const char* key_sequence;
    if (!PyArg_ParseTuple(args, "s", &key_sequence)) {
        Py_RETURN_NONE;
    }
    RimeSessionId session_id = RimeCreateSession();
    bool success = false;

    if (!RimeSimulateKeySequence(session_id, key_sequence)) {
        goto finalize;
    }

    PyObject* candidates;

    RIME_STRUCT(RimeContext, context);
    if(!RimeGetContext(session_id, &context)) {
        goto finalize;
    }

    if (!context.menu.candidates) {
        goto finalize;
    }

    candidates = PyList_New(0);
    if (candidates == NULL) {
        goto finalize;
    }

    PyObject* textKey = PyUnicode_FromString("text");
    PyObject* commentKey = PyUnicode_FromString("comment");
    PyObject* orderKey = PyUnicode_FromString("order");
    int count = 0;
    while(true) {
        for (int i = 0; i < context.menu.num_candidates; i++) {
            RimeCandidate candidate = context.menu.candidates[i];
            PyObject* dict = PyDict_New();
            PyObject* text = PyUnicode_FromString(candidate.text);
            PyObject* comment = candidate.comment ? PyUnicode_FromString(candidate.comment) : newref(Py_None);
            PyObject* order = PyLong_FromLong(count);

            PyDict_SetItem(dict, textKey, text);
            PyDict_SetItem(dict, commentKey, comment);
            PyDict_SetItem(dict, orderKey, order);

            PyList_Append(candidates, dict);

            Py_DECREF(dict);
            Py_DECREF(text);
            Py_DECREF(comment);
            Py_DECREF(order);

            count++;
        }

        if (context.menu.is_last_page) {
            break;
        }

        // Turn to next page ...
        RimeProcessKey(session_id, (int)'=', 0);
        // and grab content.
        RimeGetContext(session_id, &context);
    }

    Py_DECREF(textKey);
    Py_DECREF(commentKey);
    Py_DECREF(orderKey);

    RimeFreeContext(&context);
    success = true;

finalize:
    RimeDestroySession(session_id);
    if (success) {
        return candidates;
    } else {
        Py_XDECREF(candidates);
        Py_RETURN_NONE;
    }
}

static PyMethodDef RimeMethods[] = {
    { "get_candidates_from_keys", rime_get_candidates_from_keys, METH_VARARGS, "Get candidates from a key sequence." },
    { "init", rime_init, METH_VARARGS, "Initialize rime." },
    { NULL, NULL, 0, NULL }
};

static struct PyModuleDef rimemodule = {
    PyModuleDef_HEAD_INIT,
    "rime",
    "",
    -1,
    RimeMethods
};

PyMODINIT_FUNC
PyInit_rime(void) {
    PyObject* m = PyModule_Create(&rimemodule);
    if (m == NULL) {
        return NULL;
    }

    PyModule_AddStringConstant(m, "shared_data_dir", "/usr/share/rime-data");
    PyModule_AddStringConstant(m, "log_dir", "/tmp");
    char *data_dir;
    const char *data_home = getenv("XDG_DATA_HOME");
    if (data_home) {
        data_dir = malloc(strlen(data_home) + strlen("/pyrime") + 1);
        strcpy(data_dir, data_home);
        strcat(data_dir, "/pyrime");
    } else {
      const char *home = getenv("HOME");
      data_dir = malloc(strlen(home) + strlen("/.local/share/pyrime") + 1);
      strcpy(data_dir, home);
      strcat(data_dir, "/.local/share/pyrime");
    }
    PyModule_AddStringConstant(m, "user_data_dir", data_dir);

    return m;
}
