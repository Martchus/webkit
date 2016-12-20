/*
 * Copyright (C) 2016 Michal Dutkiewicz aka Emdek <michal@emdek.pl>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "qwebfullscreenrequest.h"

#include "qwebpage_p.h"

#include <QtCore/qpointer.h>

QT_BEGIN_NAMESPACE

class QWebFullScreenRequestPrivate {
public:
    QWebFullScreenRequestPrivate(QWebPage* page, const QUrl& origin, const QWebElement& element, bool toggleOn)
        : page(page)
        , origin(origin)
        , element(element)
        , accepted(false)
        , toggleOn(toggleOn)
    {
    }

    QPointer<QWebPage> page;
    const QUrl origin;
    QWebElement element;
    bool accepted;
    const bool toggleOn;
};

QWebFullScreenRequest::QWebFullScreenRequest(QWebPage* page, const QUrl& origin, const QWebElement& element, bool toggleOn)
    : d(new QWebFullScreenRequestPrivate(page, origin, element, toggleOn))
{
    qDebug() << Q_FUNC_INFO;
    if (element.isNull())
        d->element = page->d->fullScreenElement();
}

QWebFullScreenRequest::QWebFullScreenRequest()
{
}

QWebFullScreenRequest::QWebFullScreenRequest(const QWebFullScreenRequest& other)
    : d(new QWebFullScreenRequestPrivate(other.d->page, other.d->origin, other.d->element, other.d->toggleOn))
{
    qDebug() << Q_FUNC_INFO;
}

QWebFullScreenRequest::~QWebFullScreenRequest()
{
    qDebug() << Q_FUNC_INFO;
    if (d->accepted && d->page) {
        if (d->toggleOn) {
            qDebug() << Q_FUNC_INFO << "endEnterFullScreen";
            d->element.endEnterFullScreen();
        } else {
            qDebug() << Q_FUNC_INFO << "endExitFullScreen";
            d->element.endExitFullScreen();
            d->page->d->setFullScreenElement(QWebElement());
        }
    }
}

void QWebFullScreenRequest::accept()
{
    qDebug() << Q_FUNC_INFO;
    if (!d->page) {
        qWarning("Cannot accept QWebFullScreenRequest: Originating page is already deleted");
        return;
    }

    d->accepted = true;

    if (d->toggleOn) {
        d->page->d->setFullScreenElement(d->element);
        qDebug() << Q_FUNC_INFO << "beginEnterFullScreen";
        d->element.beginEnterFullScreen();
    } else {
        qDebug() << Q_FUNC_INFO << "beginExitFullScreen";
        d->element.beginExitFullScreen();
    }
}

void QWebFullScreenRequest::reject()
{
    qDebug() << Q_FUNC_INFO;
}

bool QWebFullScreenRequest::toggleOn() const
{
    return d->toggleOn; 
}

const QUrl &QWebFullScreenRequest::origin() const
{
    return d->origin; 
}

const QWebElement &QWebFullScreenRequest::element() const
{
    return d->element;
}

QT_END_NAMESPACE
