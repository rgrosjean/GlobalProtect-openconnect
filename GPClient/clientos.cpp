#include "clientos.h"

QStringList clientOsOptions()
{
    return {"Linux", "Windows", "Mac"};
}

QString defaultClientOs()
{
    return clientOsOptions().first();
}

int clientOsIndex(const QString &saved)
{
    int index = clientOsOptions().indexOf(saved);
    return index < 0 ? 0 : index;
}
