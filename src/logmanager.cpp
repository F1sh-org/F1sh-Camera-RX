#include "logmanager.h"
#include <QDateTime>
#include <QDebug>
#include <cstdarg>
#include <cstdio>

LogManager* LogManager::s_instance = nullptr;

LogManager* LogManager::instance()
{
    return s_instance;
}

LogManager::LogManager(QObject *parent)
    : QObject(parent)
{
    if (!s_instance) {
        s_instance = this;
    }
    
    // Add startup log
    addLogEntry("Application started");
}

LogManager::~LogManager()
{
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

QString LogManager::logText() const
{
    QMutexLocker locker(&m_mutex);
    return m_logs.join("");
}

void LogManager::log(const QString &message)
{
    if (s_instance) {
        s_instance->appendLog(message);
    } else {
        // Fallback to qDebug if no instance
        qDebug() << message;
    }
}

void LogManager::appendLog(const QString &message)
{
    addLogEntry(message);
}

void LogManager::clear()
{
    {
        QMutexLocker locker(&m_mutex);
        m_logs.clear();
        m_logCount = 0;
    }
    emit logTextChanged();
    emit logCountChanged();
}

QString LogManager::wrapText(const QString &text, int width)
{
    if (text.length() <= width) {
        return text;
    }
    
    QString result;
    int count = 0;
    int lastSpace = -1;
    
    for (int i = 0; i < text.length(); ++i) {
        QChar c = text[i];
        result.append(c);
        
        if (c == '\n') {
            count = 0;
            lastSpace = -1;
            continue;
        }
        
        if (c == ' ' || c == '\t') {
            lastSpace = result.length() - 1;
        }
        
        count++;
        
        if (count >= width) {
            if (lastSpace != -1) {
                result[lastSpace] = '\n';
                count = result.length() - lastSpace - 1;
                lastSpace = -1;
            } else {
                result.append('\n');
                count = 0;
                lastSpace = -1;
            }
        }
    }
    
    return result;
}

void LogManager::addLogEntry(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("[hh:mm:ss] ");
    QString wrapped = wrapText(message, 100);
    
    // Ensure message ends with newline
    QString logLine = timestamp + wrapped;
    if (!logLine.endsWith('\n')) {
        logLine.append('\n');
    }
    
    {
        QMutexLocker locker(&m_mutex);
        m_logs.append(logLine);
        m_logCount++;
        
        // Trim old entries if exceeding max
        while (m_logs.size() > MAX_LOG_ENTRIES) {
            m_logs.removeFirst();
        }
    }
    
    emit newLogEntry(logLine);
    emit logTextChanged();
    emit logCountChanged();
    
    // Also output to debug console
    qDebug().noquote() << logLine.trimmed();
}

// C-compatible logging functions
extern "C" {

void app_log(const char *message)
{
    if (message) {
        LogManager::log(QString::fromUtf8(message));
    }
}

void app_log_fmt(const char *format, ...)
{
    if (!format) return;
    
    char buffer[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    LogManager::log(QString::fromUtf8(buffer));
}

} // extern "C"
