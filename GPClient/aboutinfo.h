#ifndef ABOUTINFO_H
#define ABOUTINFO_H

#include <QtCore/QString>

// Address of this fork's source repository.
QString aboutRepositoryUrl();

// Rich text of the About dialog: name, version, fork lineage, licence and repository link.
QString aboutHtml(const QString &version);

#endif // ABOUTINFO_H
