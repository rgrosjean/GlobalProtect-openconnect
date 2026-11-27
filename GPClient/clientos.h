#ifndef CLIENTOS_H
#define CLIENTOS_H

#include <QtCore/QString>
#include <QtCore/QStringList>

// The Client OS values the Settings dialog offers, in display order.
// The first one (Linux) is also the default.
QStringList clientOsOptions();

// Index in clientOsOptions() that a saved Client OS selects.
// A value that is not one of the options selects Linux.
int clientOsIndex(const QString &saved);

#endif // CLIENTOS_H
