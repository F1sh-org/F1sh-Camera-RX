#ifndef LOGMANAGER_H
#define LOGMANAGER_H

#ifdef __cplusplus
#include <QObject>
#include <QString>
#include <QStringList>
#include <QMutex>

class LogManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString logText READ logText NOTIFY logTextChanged)
    Q_PROPERTY(int logCount READ logCount NOTIFY logCountChanged)

public:
    static LogManager* instance();
    
    explicit LogManager(QObject *parent = nullptr);
    ~LogManager();

    QString logText() const;
    int logCount() const { return m_logCount; }
    
    // Static log function that can be called from anywhere
    static void log(const QString &message);
    
    Q_INVOKABLE void clear();
    Q_INVOKABLE void appendLog(const QString &message);

signals:
    void logTextChanged();
    void logCountChanged();
    void newLogEntry(const QString &message);

private:
    void addLogEntry(const QString &message);
    QString wrapText(const QString &text, int width);
    
    QStringList m_logs;
    int m_logCount = 0;
    mutable QMutex m_mutex;
    
    static const int MAX_LOG_ENTRIES = 1000;
    static LogManager* s_instance;
};

// Convenience macro for logging (C++)
#define APP_LOG(msg) LogManager::log(msg)

extern "C" {
#endif

// C-compatible logging function (can be called from C code)
void app_log(const char *message);
void app_log_fmt(const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif // LOGMANAGER_H
