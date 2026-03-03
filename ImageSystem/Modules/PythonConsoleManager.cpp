#include "PythonConsoleManager.h"
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <QDebug>

namespace py = pybind11;

PythonConsoleManager::PythonConsoleManager(QObject* parent)
    : QObject(parent)
    , m_isExecuting(false)
{
}

void PythonConsoleManager::executeCommand(const QString& command)
{
    if (command.trimmed().isEmpty() || m_isExecuting)
        return;

    m_history.append(command);
    setisExecuting(true);
    emit outputAppended(">>> " + command + "\n");

    QtConcurrent::run([this, command]() {
        doExecute(command);
        QMetaObject::invokeMethod(this, [this]() {
            setisExecuting(false);
        }, Qt::QueuedConnection);
    });
}

void PythonConsoleManager::doExecute(const QString& cmd)
{
    py::gil_scoped_acquire acquire;

    try {
        // pip commands -> subprocess
        QString trimmed = cmd.trimmed();
        bool isPipCmd = trimmed.startsWith("pip ") || trimmed == "pip";

        std::string setupCapture = R"(
import sys as _sys
import io as _io
_old_stdout = _sys.stdout
_old_stderr = _sys.stderr
_sys.stdout = _io.StringIO()
_sys.stderr = _io.StringIO()
)";

        std::string teardown = R"(
_captured_out = _sys.stdout.getvalue()
_captured_err = _sys.stderr.getvalue()
_sys.stdout = _old_stdout
_sys.stderr = _old_stderr
)";

        py::exec(setupCapture);

        std::string code;
        if (isPipCmd) {
            QStringList parts = trimmed.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            std::string argsStr = "[";
            for (int i = 0; i < parts.size(); ++i) {
                if (i > 0) argsStr += ", ";
                argsStr += "\"" + parts[i].toStdString() + "\"";
            }
            argsStr += "]";

            code =
                "import subprocess, sys\n"
                "try:\n"
                "    _r = subprocess.run([sys.executable, \"-m\"] + " + argsStr + ", capture_output=True, text=True, timeout=120)\n"
                "    if _r.stdout:\n"
                "        print(_r.stdout, end='')\n"
                "    if _r.returncode != 0 and _r.stderr:\n"
                "        print(_r.stderr, end='', file=_sys.stderr)\n"
                "except Exception as _e:\n"
                "    print(str(_e), file=_sys.stderr)\n";
        } else {
            // Try eval first (for expressions), fall back to exec (for statements)
            code =
                "try:\n"
                "    _result = eval(" + py::repr(py::str(cmd.toStdString())).cast<std::string>() + ")\n"
                "    if _result is not None:\n"
                "        print(repr(_result))\n"
                "except SyntaxError:\n"
                "    exec(" + py::repr(py::str(cmd.toStdString())).cast<std::string>() + ")\n"
                "except Exception as _e:\n"
                "    print(str(_e), file=_sys.stderr)\n";
        }

        try {
            py::exec(code);
        } catch (py::error_already_set& e) {
            // Write error into captured stderr
            try {
                std::string errMsg = e.what();
                py::exec("print(" + py::repr(py::str(errMsg)).cast<std::string>() + ", file=_sys.stderr)");
            } catch (...) {}
        }

        py::exec(teardown);

        std::string out = py::eval("_captured_out").cast<std::string>();
        std::string err = py::eval("_captured_err").cast<std::string>();

        QString output;
        if (!out.empty())
            output += QString::fromStdString(out);
        if (!err.empty())
            output += QString::fromStdString(err);

        if (!output.isEmpty()) {
            if (!output.endsWith('\n'))
                output += '\n';
            QMetaObject::invokeMethod(this, [this, output]() {
                emit outputAppended(output);
            }, Qt::QueuedConnection);
        }

    } catch (std::exception& e) {
        // Restore stdout/stderr in case of catastrophic failure
        try {
            py::exec(R"(
import sys as _sys
if hasattr(_sys, '_old_stdout') and _sys._old_stdout:
    _sys.stdout = _sys._old_stdout
if hasattr(_sys, '_old_stderr') and _sys._old_stderr:
    _sys.stderr = _sys._old_stderr
)");
        } catch (...) {}

        QString errMsg = QString::fromUtf8(e.what()) + "\n";
        QMetaObject::invokeMethod(this, [this, errMsg]() {
            emit outputAppended(errMsg);
        }, Qt::QueuedConnection);
    }
}

void PythonConsoleManager::clearOutput()
{
    emit outputAppended("\x1B[CLEAR]");
}

QString PythonConsoleManager::getHistory(int index) const
{
    if (index >= 0 && index < m_history.size())
        return m_history[index];
    return {};
}

int PythonConsoleManager::historySize() const
{
    return m_history.size();
}
