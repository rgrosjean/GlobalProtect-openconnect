#include "aboutinfo.h"

QString aboutRepositoryUrl()
{
    return QStringLiteral("https://github.com/rgrosjean/GlobalProtect-openconnect");
}

QString aboutHtml(const QString &version)
{
    return QStringLiteral(
        "<h3>GlobalProtect</h3>"
        "<p>Version %1</p>"
        "<p>A fork of kakwa/GlobalProtect-openconnect, itself a fork of the "
        "1.x branch of yuezk/GlobalProtect-openconnect.</p>"
        "<p>Licensed under the GPL-3.0.</p>"
        "<p><a href=\"%2\">%2</a></p>"
    ).arg(version.toHtmlEscaped(), aboutRepositoryUrl());
}
