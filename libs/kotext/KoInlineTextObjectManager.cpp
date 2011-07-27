/* This file is part of the KDE project
 * Copyright (C) 2006-2009 Thomas Zander <zander@kde.org>
 * Copyright (C) 2008 Thorsten Zachmann <zachmann@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#include "KoInlineTextObjectManager.h"
#include "InsertNamedVariableAction_p.h"
#include "InsertTextReferenceAction_p.h"
#include "InsertTextLocator_p.h"
#include "KoInlineObjectRegistry.h"
#include "KoTextLocator.h"
#include "KoBookmark.h"
#include "KoInlineNote.h"

#include <QTextCursor>
#include <QPainter>
#include <QTextDocument>

KoInlineTextObjectManager::KoInlineTextObjectManager(QObject *parent)
        : QObject(parent),
        m_lastObjectId(0),
        m_variableManager(this)
{
}

KoInlineTextObjectManager::~KoInlineTextObjectManager()
{
}

KoInlineObject *KoInlineTextObjectManager::inlineTextObject(const QTextCharFormat &format) const
{
    int id = format.intProperty(InlineInstanceId);
    if (id <= 0)
        return 0;
    return m_objects.value(id, 0);
}

KoInlineObject *KoInlineTextObjectManager::inlineTextObject(const QTextCursor &cursor) const
{
    return inlineTextObject(cursor.charFormat());
}

KoInlineObject *KoInlineTextObjectManager::inlineTextObject(int id) const
{
    return m_objects.value(id);
}

void KoInlineTextObjectManager::insertInlineObject(QTextCursor &cursor, KoInlineObject *object)
{
    QTextCharFormat oldCf = cursor.charFormat();
    // create a new format out of the old so that the current formatting is
    // also used for the inserted object.  KoVariables render text too ;)
    QTextCharFormat cf(oldCf);
    cf.setObjectType(QTextFormat::UserObject + 1);
    cf.setProperty(InlineInstanceId, ++m_lastObjectId);
    cursor.insertText(QString(QChar::ObjectReplacementCharacter), cf);
    object->setId(m_lastObjectId);
    m_objects.insert(m_lastObjectId, object);
    object->setManager(this);
    object->setup();
    if (object->propertyChangeListener()) {
        m_listeners.append(object);
        QHash<int, QVariant>::iterator i;
        for (i = m_properties.begin(); i != m_properties.end(); ++i)
            object->propertyChanged((KoInlineObject::Property)(i.key()), i.value());
    }

    KoBookmark *bookmark = dynamic_cast<KoBookmark *>(object);
    if (bookmark
            && (bookmark->type() == KoBookmark::StartBookmark
                || bookmark->type() == KoBookmark::SinglePosition))
        m_bookmarkManager.insert(bookmark->name(), bookmark);

    // reset to use old format so that the InlineInstanceId is no longer set.
    cursor.setCharFormat(oldCf);
}

bool KoInlineTextObjectManager::removeInlineObject(QTextCursor &cursor)
{
    KoInlineObject *object = inlineTextObject(cursor);
    if (object->propertyChangeListener()) {
        int position = m_listeners.indexOf(object);
        m_listeners.removeAt(position);
    }

    // what if a KoTextLocator is removed? what to do with KoTextReference?
    QTextCharFormat format = cursor.charFormat();
    int id = format.intProperty(InlineInstanceId);
    if (id <= 0)
        return false;

    int removed = m_objects.remove(id);

    KoBookmark *bookmark = dynamic_cast<KoBookmark *>(object);
    if (bookmark) {
        if (bookmark->type() == KoBookmark::StartBookmark) {
            m_bookmarkManager.remove(bookmark->name());
            KoBookmark *endBookmark = bookmark->endBookmark();
            endBookmark->setType(KoBookmark::SinglePosition);
            m_bookmarkManager.insert(bookmark->name(), endBookmark);
        } else if (bookmark->type() == KoBookmark::EndBookmark) {
            KoBookmark *startBookmark = m_bookmarkManager.retrieveBookmark(bookmark->name());
            startBookmark->setType(KoBookmark::SinglePosition);
        } else
            m_bookmarkManager.remove(bookmark->name());
    }

    delete object;
    object = 0;

    if (removed != 0) {
        cursor.deletePreviousChar();
        return true;
    }
    return false;
}

void KoInlineTextObjectManager::removeInlineObject(KoInlineObject *object)
{
    if (object) {
        m_objects.remove(object->id());
    }
    // TODO dirty the document somehow
}

void KoInlineTextObjectManager::setProperty(KoInlineObject::Property key, const QVariant &value)
{
    if (m_properties.contains(key)) {
        if (value == m_properties.value(key))
            return;
        m_properties.remove(key);
    }
    m_properties.insert(key, value);
    foreach (KoInlineObject *obj, m_listeners)
        obj->propertyChanged(key, value);
}

QVariant KoInlineTextObjectManager::property(KoInlineObject::Property key) const
{
    return m_properties.value(key);
}

int KoInlineTextObjectManager::intProperty(KoInlineObject::Property key) const
{
    if (!m_properties.contains(key))
        return 0;
    return m_properties.value(key).toInt();
}

bool KoInlineTextObjectManager::boolProperty(KoInlineObject::Property key) const
{
    if (!m_properties.contains(key))
        return false;
    return m_properties.value(key).toBool();
}

QString KoInlineTextObjectManager::stringProperty(KoInlineObject::Property key) const
{
    if (!m_properties.contains(key))
        return QString();
    return qvariant_cast<QString>(m_properties.value(key));
}

const KoVariableManager *KoInlineTextObjectManager::variableManager() const
{
    return &m_variableManager;
}

KoVariableManager *KoInlineTextObjectManager::variableManager()
{
    return &m_variableManager;
}

KoBookmarkManager *KoInlineTextObjectManager::bookmarkManager()
{
    return &m_bookmarkManager;
}

void KoInlineTextObjectManager::removeProperty(KoInlineObject::Property key)
{
    m_properties.remove(key);
}

QList<QAction*> KoInlineTextObjectManager::createInsertVariableActions(KoCanvasBase *host) const
{
    QList<QAction *> answer = KoInlineObjectRegistry::instance()->createInsertVariableActions(host);
    int i = 0;
    foreach(const QString & name, m_variableManager.variables()) {
        answer.insert(i++, new InsertNamedVariableAction(host, this, name));
    }

    answer.append(new InsertTextLocator(host));
    answer.append(new InsertTextReferenceAction(host, this));
    return answer;
}

QList<KoTextLocator*> KoInlineTextObjectManager::textLocators() const
{
    QList<KoTextLocator*> answers;
    foreach(KoInlineObject *object, m_objects) {
        KoTextLocator *tl = dynamic_cast<KoTextLocator*>(object);
        if (tl)
            answers.append(tl);
    }
    return answers;
}

QList<KoInlineNote*> KoInlineTextObjectManager::footNotes() const
{
    QList<KoInlineNote*> answers;
    foreach(KoInlineObject* object, m_objects) {
        KoInlineNote* note = dynamic_cast<KoInlineNote*>(object);
        if (note && note->type() == KoInlineNote::Footnote) {
            if(note->autoNumbering())
                answers.append(note);
        }
    }
    return answers;
}

void KoInlineTextObjectManager::reNumbering(QTextBlock block)
{
    int i=1,j=1;

    while(block.isValid()) {
        QString text = block.text();
        int pos = text.indexOf(QChar::ObjectReplacementCharacter);

        while (pos >= 0 && pos <= block.length() ) {
            QTextCursor c1(block);
            c1.setPosition(block.position() + pos);
            c1.setPosition(c1.position() + 1, QTextCursor::KeepAnchor);

            KoInlineNote *note = dynamic_cast<KoInlineNote*>(this->inlineTextObject(c1));
            if (note && note->type() == KoInlineNote::Footnote) {
                if(note->autoNumbering()) {
                    note->setLabel(QString().number(i));

                    QTextCursor cursor = note->textFrame()->firstCursorPosition();
                    cursor.movePosition(QTextCursor::WordRight,QTextCursor::KeepAnchor);
                    cursor.removeSelectedText();
                    QTextCharFormat *fmat = new QTextCharFormat();
                    fmat->setVerticalAlignment(QTextCharFormat::AlignSuperScript);
                    if(note->autoNumbering()) {
                        cursor.insertText(note->label(),*fmat);
                    }
                    fmat->setVerticalAlignment(QTextCharFormat::AlignNormal);
                    cursor.insertText(" ",*fmat);
                    i++;
                }
            }
            else if (note && note->type() == KoInlineNote::Endnote) {
                if(note->autoNumbering()) {
                    note->setLabel(QString().number(j));

                    QTextCursor cursor = note->textFrame()->firstCursorPosition();
                    cursor.movePosition(QTextCursor::WordRight,QTextCursor::KeepAnchor);
                    cursor.removeSelectedText();
                    QTextCharFormat *fmat = new QTextCharFormat();
                    fmat->setVerticalAlignment(QTextCharFormat::AlignSuperScript);
                    if(note->autoNumbering()) {
                        cursor.insertText(note->label(),*fmat);
                    }
                    fmat->setVerticalAlignment(QTextCharFormat::AlignNormal);
                    cursor.insertText(" ",*fmat);
                    j++;
                }
            }

            pos = text.indexOf(QChar::ObjectReplacementCharacter, pos + 1);
        }
        block = block.next();
    }
}

QList<KoInlineNote*> KoInlineTextObjectManager::endNotes() const
{
    QList<KoInlineNote*> answers;
    foreach(KoInlineObject* object, m_objects) {
        KoInlineNote* note = dynamic_cast<KoInlineNote*>(object);
        if (note && note->type() == KoInlineNote::Endnote) {
            answers.append(note);
        }
    }
    return answers;
}

int KoInlineTextObjectManager::displayedNotes(QTextBlock block) const
{
    int i=1;

    while(block.isValid()) {
        QString text = block.text();
        int pos = text.indexOf(QChar::ObjectReplacementCharacter);

        while (pos >= 0 && pos <= block.length() ) {
            QTextCursor c1(block);
            c1.setPosition(block.position() + pos);
            c1.setPosition(c1.position() + 1, QTextCursor::KeepAnchor);

            KoInlineNote *note = dynamic_cast<KoInlineNote*>(this->inlineTextObject(c1));
            if ((note && note->type() == KoInlineNote::Footnote)||(note && note->type() == KoInlineNote::Endnote)) {
                if(note->autoNumbering()) {
                    i++;
                }
            }

            pos = text.indexOf(QChar::ObjectReplacementCharacter, pos + 1);
        }
        block = block.next();
    }
    return i;
}

KoInlineNote *KoInlineTextObjectManager::getFirstNote(QTextBlock block) const
{
    while(block.isValid()) {
        QString text = block.text();
        int pos = text.indexOf(QChar::ObjectReplacementCharacter);

        while (pos >= 0 && pos <= block.length() ) {
            QTextCursor c1(block);
            c1.setPosition(block.position() + pos);
            c1.setPosition(c1.position() + 1, QTextCursor::KeepAnchor);

            KoInlineNote *note = dynamic_cast<KoInlineNote*>(this->inlineTextObject(c1));
            if (note && note->type() == KoInlineNote::Footnote) {
                return note;
            }
            else if (note && note->type() == KoInlineNote::Endnote) {
                return note;
            }

            pos = text.indexOf(QChar::ObjectReplacementCharacter, pos + 1);
        }
        block = block.next();
    }
}

void KoInlineTextObjectManager::documentInformationUpdated(const QString &info, const QString &data)
{
    if (info == "title")
        setProperty(KoInlineObject::Title, data);
    else if (info == "description")
        setProperty(KoInlineObject::Description, data);
    else if (info == "subject")
        setProperty(KoInlineObject::Subject, data);
    else if (info == "keyword")
        setProperty(KoInlineObject::Keywords, data);
    else if (info == "creator")
        setProperty(KoInlineObject::AuthorName, data);
    else if (info == "initial")
        setProperty(KoInlineObject::AuthorInitials, data);
    else if (info == "author-title")
        setProperty(KoInlineObject::SenderTitle, data);
    else if (info == "email")
        setProperty(KoInlineObject::SenderEmail, data);
    else if (info == "telephone")
        setProperty(KoInlineObject::SenderPhonePrivate, data);
    else if (info == "telephone-work")
        setProperty(KoInlineObject::SenderPhoneWork, data);
    else if (info == "fax")
        setProperty(KoInlineObject::SenderFax, data);
    else if (info == "country")
        setProperty(KoInlineObject::SenderCountry, data);
    else if (info == "postal-code")
        setProperty(KoInlineObject::SenderPostalCode, data);
    else if (info == "city")
        setProperty(KoInlineObject::SenderCity, data);
    else if (info == "street")
        setProperty(KoInlineObject::SenderStreet, data);
    else if (info == "position")
        setProperty(KoInlineObject::SenderPosition, data);
    else if (info == "company")
        setProperty(KoInlineObject::SenderCompany, data);
}

QList<KoInlineObject*> KoInlineTextObjectManager::inlineTextObjects() const
{
    return m_objects.values();
}

#include <KoInlineTextObjectManager.moc>
